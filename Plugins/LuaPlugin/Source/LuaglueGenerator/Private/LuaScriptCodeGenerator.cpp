// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "LuaglueGeneratorPrivatePCH.h"
#include "LuaScriptCodeGenerator.h"
#define GetWeakObjType(varname, returnname) 		FString typeName = GetPropertyTypeCPP(varname, CPPF_ArgumentOrReturnValue); \
		int FirstSpaceIndex = typeName.Find(TEXT("<"));\
		typeName = typeName.Mid(FirstSpaceIndex + 1);\
		typeName.RemoveAt(typeName.Len() - 1);\
		FString returnname = FString::Printf(TEXT("TWeakObjectPtr_%s"), *typeName);
		
FLuaScriptCodeGenerator::FLuaScriptCodeGenerator(const FString& RootLocalPath, const FString& RootBuildPath, const FString& OutputDirectory, const FString& InIncludeBase)
	: FScriptCodeGeneratorBase(RootLocalPath, RootBuildPath, OutputDirectory, InIncludeBase)
{
	WeakPtrClass.Add("UObject");
	FString configPath = InIncludeBase / ".." / ".." / "Config" / "luaconfig.ini";
	GConfig->GetArray(TEXT("Lua"), TEXT("SupportedStruct"), SupportedStruct, configPath);
	GConfig->GetArray(TEXT("Lua"), TEXT("NoPropertyStruct"), NoexportPropertyStruct, configPath);
}

FString FLuaScriptCodeGenerator::GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, FString classname, UFunction* Function)
{
	return GenerateWrapperFunctionDeclaration(ClassNameCPP, classname, Function->GetName());
}

FString FLuaScriptCodeGenerator::GenerateWrapperFunctionDeclaration(const FString& ClassNameCPP, FString classname, const FString& FunctionName)
{
	int i = 0;
	FString funcname = FString::Printf(TEXT("static int32 %s_%s(lua_State* L)"), *ClassNameCPP, *FunctionName, i);
	while (true)
	{
		if (ExportedFunc.Contains(funcname) == false)
			break;
		funcname = FString::Printf(TEXT("static int32 %s_%s%d(lua_State* L)"), *ClassNameCPP, *FunctionName, i);
	}
	ExportedFunc.Add(funcname);
	return funcname;
}

FString FLuaScriptCodeGenerator::InitializeParam(UProperty* Param, int32 ParamIndex, bool isnotpublicproperty, FString arrayName)
{
	FString Initializer;

	if (!(Param->GetPropertyFlags() & CPF_ReturnParm))
	{
		// In Lua, the first param index on the stack is 1 and it's the object we're invoking the function on
		ParamIndex += 2;

		if (Param->IsA(UIntProperty::StaticClass()))
		{
			Initializer = TEXT("(luaL_checkint");
		}
		else if (Param->IsA(UFloatProperty::StaticClass()))
		{
			Initializer = TEXT("(float)(luaL_checknumber");
		}
		else if (Param->IsA(UStrProperty::StaticClass()))
		{
			Initializer = TEXT("ANSI_TO_TCHAR(luaL_checkstring");
		}
		else if (Param->IsA(UNameProperty::StaticClass()))
		{
			Initializer = TEXT("FName(luaL_checkstring");
		}
		else if (Param->IsA(UBoolProperty::StaticClass()))
		{
			Initializer = TEXT("!!(lua_toboolean");
		}
		else if (Param->IsA(UClassProperty::StaticClass()))
		{
			//FString typeName = GetPropertyTypeCPP(Param, CPPF_Implementation);
			Initializer = TEXT("(UClass*)(UTableUtil::tousertype(\"UClass\",");
			return FString::Printf(TEXT("%s %d))"), *Initializer, ParamIndex);
		}
		else if (Param->IsA(UArrayProperty::StaticClass()))
		{
			auto PropertyArr = Cast<UArrayProperty>(Param);
			Initializer += FString::Printf(TEXT("\t%s.SetNum(lua_objlen(L, %d));\r\n"), *arrayName, ParamIndex);

			Initializer += FString::Printf(TEXT("\tlua_pushnil(L);\r\n"));
			Initializer += FString::Printf(TEXT("\twhile (lua_next(L, %d) != 0) {\r\n"), ParamIndex);
			Initializer += FString::Printf(TEXT("\t\tint32 i = lua_tointeger(L, -2)-1;\r\n"));
			FString to = InitializeParam(PropertyArr->Inner, -3);
			if (PropertyArr->Inner->IsA(UStructProperty::StaticClass()))
				Initializer += FString::Printf(TEXT("\t\t%s[i] = *%s;\r\n"), *arrayName, *to);
			else
				Initializer += FString::Printf(TEXT("\t\t%s[i] = %s;\r\n"), *arrayName, *to);
			Initializer += FString::Printf(TEXT("\t\tlua_pop(L, 1);\r\n\t}\r\n"));
			return Initializer;
		}
		else if (Param->IsA(UWeakObjectProperty::StaticClass()))
		{
			GetWeakObjType(Param, nomeaning)
			Initializer = FString::Printf(TEXT("(%s*)(UTableUtil::tousertype(\"%s\","), *typeName, *typeName);
			return FString::Printf(TEXT("%s %d))"), *Initializer, ParamIndex);
		}
		else if (Param->IsA(UObjectPropertyBase::StaticClass()) || Param->IsA(UStructProperty::StaticClass()))
		{
			FString typeName = GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue);
			if (typeName.Contains("*"))
			{
				FString luatypename = GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue);
				luatypename.RemoveAt(luatypename.Len() - 1);
				Initializer = FString::Printf(TEXT("(%s)(UTableUtil::tousertype(\"%s\","), *typeName, *luatypename);
			}
			else if (isnotpublicproperty)
			{
				Initializer = FString::Printf(TEXT("(%s*)(UTableUtil::tousertype(\"%s\","), *typeName, *typeName);
			}
			else
			{
				Initializer = FString::Printf(TEXT("(%s*)(UTableUtil::tousertype(\"%s\","), *typeName, *typeName);
			}
			return FString::Printf(TEXT("%s %d))"), *Initializer, ParamIndex);
		}
		else if (Param->IsA(UByteProperty::StaticClass()))
		{
			FString typeName = GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue);
			Initializer = FString::Printf(TEXT("(%s)(luaL_checkint"), *typeName);
		}
		else
		{
			Initializer = TEXT("(luaL_checkint");
		}
	}
	return FString::Printf(TEXT("%s(L, %d))"), *Initializer, ParamIndex);
}

FString FLuaScriptCodeGenerator::InitializeFunctionDispatchParam(UFunction* Function, UProperty* Param, int32 ParamIndex)
{
	if (!(Param->GetPropertyFlags() & CPF_ReturnParm))
	{
		//return FString::Printf(TEXT("%s(L, %d))"), *Initializer, ParamIndex);
		return InitializeParam(Param, ParamIndex);
	}
	else
	{
		return FScriptCodeGeneratorBase::InitializeFunctionDispatchParam(Function, Param, ParamIndex);
	}
}

FString FLuaScriptCodeGenerator::GenerateObjectDeclarationFromContext(const FString& ClassNameCPP)
{
	return FString::Printf(TEXT("%s* Obj = (%s*)UTableUtil::tousertype(\"%s\",1);"), *ClassNameCPP, *ClassNameCPP, *ClassNameCPP);
}

FString FLuaScriptCodeGenerator::Push(const FString& ClassNameCPP, UFunction* Function, UProperty* ReturnValue, FString name, bool bConsiderArrayDim)
{
	FString Initializer;
	if (ReturnValue->ArrayDim > 1 && bConsiderArrayDim)
	{
		FString CreateTableCode;
		// CreateTableCode = FString::Printf(TEXT("int32 len = sizeof(%s)/sizeof(%s);\r\n"), *name, *GetPropertyTypeCPP(ReturnValue, CPPF_ArgumentOrReturnValue));
		CreateTableCode = FString::Printf(TEXT("int32 len = CPP_ARRAY_DIM(%s,%s);\r\n"), *ReturnValue->GetName(), *ClassNameCPP);
		CreateTableCode += "\tlua_newtable(L);\r\n";
		CreateTableCode += "\tfor(int32 i = 0; i < len; ++i)\r\n\t{\r\n";
		CreateTableCode += "\t\tlua_pushinteger(L, i+1);\r\n";
		FString pushvalueName = FString::Printf(TEXT("%s[i]"), *name);
		FString pushCode = Push(ClassNameCPP, Function, ReturnValue, pushvalueName, false);
		CreateTableCode += FString::Printf(TEXT("\t\t%s\r\n"), *pushCode);
		CreateTableCode += "\t\tlua_rawset(L, -3);\r\n\t}\r\n";
		Initializer = CreateTableCode;
	}
	else if (ReturnValue->IsA(UIntProperty::StaticClass()) || ReturnValue->IsA(UInt8Property::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("lua_pushinteger(L, %s);"), *name);
	}
	else if (ReturnValue->IsA(UFloatProperty::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("lua_pushnumber(L, %s);"), *name);
	}
	else if (ReturnValue->IsA(UStrProperty::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("lua_pushstring(L, TCHAR_TO_ANSI(*%s));"), *name);
	}
	else if (ReturnValue->IsA(UNameProperty::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("lua_pushstring(L, TCHAR_TO_ANSI(*%s.ToString()));"), *name);
	}
	else if (ReturnValue->IsA(UBoolProperty::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("lua_pushboolean(L, %s);"), *name);
	}
	else if (ReturnValue->IsA(UClassProperty::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("UTableUtil::pushclass(\"UClass\", (void*)(%s));"), *name);
	}
	else if (ReturnValue->IsA(UStructProperty::StaticClass()))
	{
		FString typeName = GetPropertyTypeCPP(ReturnValue, CPPF_ArgumentOrReturnValue);
		if (Function) 
			Initializer = FString::Printf(TEXT("UTableUtil::pushclass(\"%s\", (void*)(new %s(%s)), true);"), *typeName, *typeName, *name);
		else
			Initializer = FString::Printf(TEXT("UTableUtil::pushclass(\"%s\", (void*)(%s));"), *typeName, *name);
	}
	else if (ReturnValue->IsA(UWeakObjectProperty::StaticClass()))
	{
		GetWeakObjType(ReturnValue, objtype)
		Initializer = FString::Printf(TEXT("UTableUtil::pushclass(\"%s\", (void*)(new %s(%s)), true);"), *objtype, *objtype, *name);
		WeakPtrClass.Add(typeName);
	}
	else if (ReturnValue->IsA(UObjectPropertyBase::StaticClass()))
	{
		FString typeName = GetPropertyTypeCPP(ReturnValue, CPPF_ArgumentOrReturnValue);
		if (typeName.Contains("*"))
		{
			FString luatypeName = typeName;
			luatypeName.RemoveAt(luatypeName.Len() - 1);
			Initializer = FString::Printf(TEXT("UTableUtil::pushclass(\"%s\", (void*)%s);"), *luatypeName, *name);
		}
		else
		{
			Initializer = FString::Printf(TEXT("UTableUtil::pushclass(\"%s\", (void*)(new %s(%s)), holyshit);"), *typeName, *typeName, *name);
		}
	}
	else if (ReturnValue->IsA(UArrayProperty::StaticClass()))
	{
		auto PropertyArr = Cast<UArrayProperty>(ReturnValue);
		FString innerType = GetPropertyTypeCPP(PropertyArr->Inner, CPPF_ArgumentOrReturnValue);
		FString CreateTableCode;
		CreateTableCode += "\tlua_newtable(L);\r\n";
		CreateTableCode += FString::Printf(TEXT("\tfor(int32 i = 0; i < %s.Num(); ++i)\r\n\t{\r\n"), *name);;
		CreateTableCode += "\t\tlua_pushinteger(L, i+1);\r\n";
		FString pushvalueName = FString::Printf(TEXT("%s[i]"), *name);
		FString pushCode = Push(ClassNameCPP, (UFunction*)1, PropertyArr->Inner, pushvalueName);
		if (Function == nullptr && !(ReturnValue->PropertyFlags & CPF_NativeAccessSpecifierPublic))
		{
			FString typecast = GetPropertyCastType(PropertyArr->Inner);
			if (typecast.IsEmpty())
				typecast = innerType;
			FString statictype = GetPropertyType(PropertyArr->Inner);
			CreateTableCode += FString::Printf(TEXT("\t\t%s* InnerProperty = Cast<%s>(p->Inner);\r\n"), *statictype, *statictype);
			CreateTableCode += FString::Printf(TEXT("\t\t%s temp = (%s)InnerProperty->%s(%s.GetRawPtr(i));\r\n"), *typecast, *typecast, *GetPropertyGetFunc(PropertyArr->Inner), *name);
			pushvalueName = FString::Printf(TEXT("temp"));
			pushCode = Push(ClassNameCPP, Function, PropertyArr->Inner, pushvalueName);
		}
		CreateTableCode += FString::Printf(TEXT("\t\t%s\r\n"), *pushCode);
		CreateTableCode += "\t\tlua_rawset(L, -3);\r\n\t}\r\n";
		Initializer = CreateTableCode;
	}
	else if (ReturnValue->IsA(UByteProperty::StaticClass()))
	{
		Initializer = FString::Printf(TEXT("lua_pushinteger(L, (int)%s);"), *name);
	}
	else
	{
		Initializer = FString::Printf(TEXT("lua_pushinteger(L, %s);"), *name);
		//FError::Throwf(TEXT("Unsupported function return type: %s"), *ReturnValue->GetClass()->GetName());
	}
	return Initializer;
}

FString FLuaScriptCodeGenerator::GenerateReturnValueHandler(const FString& ClassNameCPP, UFunction* Function, UProperty* ReturnValue)
{
	if (ReturnValue)
	{
		FString returnValueName = FString("result");
		return FString::Printf(TEXT("%s\r\n\treturn 1;"), *Push(ClassNameCPP, Function, ReturnValue, returnValueName));
	}
	else
	{
		return TEXT("return 0;");
	}
}

bool FLuaScriptCodeGenerator::CanExportClass(UClass* Class)
{
	bool bCanExport = FScriptCodeGeneratorBase::CanExportClass(Class);
	if (bCanExport)
	{
		const FString ClassNameCPP = GetClassNameCPP(Class);
		// No functions to export? Don't bother exporting the class.
		bool bHasMembersToExport = false;
		for (TFieldIterator<UFunction> FuncIt(Class); !bHasMembersToExport && FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (CanExportFunction(ClassNameCPP, Class, Function))
			{
				bHasMembersToExport = true;
			}
		}
		// Check properties too
		if (!bHasMembersToExport)
		{
			for (TFieldIterator<UProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); !bHasMembersToExport && PropertyIt; ++PropertyIt)
			{
				UProperty* Property = *PropertyIt;
				if (CanExportProperty(ClassNameCPP, Class, Property))
				{
					bHasMembersToExport = true;
				}
			}
		}
		bCanExport = bHasMembersToExport;
	}
	return bCanExport;
}

bool FLuaScriptCodeGenerator::CanExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	if (Function->GetName().Contains("DEPRECATED"))
		return false;
	if (Function->GetName() == "Blueprint_GetSizeX" ||
		Function->GetName() == "Blueprint_GetSizeY" ||
		Function->GetName() == "ContainsEmitterType" ||
		Function->GetName() == "StackTrace" ||
		Function->GetName() == "SetInnerConeAngle" ||
		Function->GetName() == "SetOuterConeAngle" ||
		Function->GetName() == "OnInterpToggle" ||
		Function->GetName() == "StartPrecompute" ||
		Function->GetName() == "OnRep_Timeline" ||
		(Function->GetName() == "CreateInstance" && (ClassNameCPP == "ULevelStreaming" || ClassNameCPP == "ULevelStreamingAlwaysLoaded" || ClassNameCPP == "ULevelStreamingKismet")) ||
		Function->GetName() == "SetAreaClass" ||
		Function->GetName() == "PaintVerticesSingleColor" ||
		Function->GetName() == "RemovePaintedVertices" ||
		Function->GetName() == "LogText" ||
		Function->GetName() == "LogLocation" ||
		(ClassNameCPP == "UVisualLoggerKismetLibrary" && Function->GetName() == "LogBox") ||
		(ClassNameCPP == "UMeshVertexPainterKismetLibrary" && Function->GetName() == "PaintVerticesLerpAlongAxis") ||
		(Function->GetName() == "MakeStringAssetReference") ||
		(ClassNameCPP == "ULevelStreamingKismet" && Function->GetName() == "LoadLevelInstance")
		)
	{
		return false;
	}

	bool bExport = FScriptCodeGeneratorBase::CanExportFunction(ClassNameCPP, Class, Function);
	if (bExport)
	{
		for (TFieldIterator<UProperty> ParamIt(Function); bExport && ParamIt; ++ParamIt)
		{
			UProperty* Param = *ParamIt;
			if (!IsPropertyTypeSupported(Param))
				return false;
		}
	}
	return bExport;
}

FString CallCode(UFunction* Function, bool bIsStaticFunc, bool hasresult, int num, FString paramlist, FString ClassNameCPP, bool isfinal = false)
{
	if (!paramlist.IsEmpty())
		paramlist.RemoveAt(paramlist.Len() - 1);
	if (!bIsStaticFunc)
		num = num + 1;
	FString code;
	if (!isfinal)
		code += FString::Printf(TEXT("\tif (num_params == %d )\r\n\t{\r\n"), num);
	if (bIsStaticFunc)
		if (hasresult)
			code += FString::Printf(TEXT("\t\tresult = %s::%s(%s);\r\n"), *ClassNameCPP, *Function->GetName(), *paramlist);
		else
			code += FString::Printf(TEXT("\t\t%s::%s(%s);\r\n"), *ClassNameCPP, *Function->GetName(), *paramlist);
	else
		if (hasresult)
			code += FString::Printf(TEXT("\t\tresult = Obj->%s(%s);\r\n"), *Function->GetName(), *paramlist);
		else
			code += FString::Printf(TEXT("\t\tObj->%s(%s);\r\n"), *Function->GetName(), *paramlist);
	if (!isfinal)
		code += "\t\tgoto end;\r\n\t}\r\n";
	return code;
}

FString FLuaScriptCodeGenerator::GenerateFunctionDispatch(UFunction* Function, const FString &ClassNameCPP, bool bIsStaticFunc)
{
	FString Params;
	FString paramList;
	FString returnType;
	FString FuncName = Function->GetName();
	int count_all_param = 0;
	int count_default_param = 0;
	int index_first_default = 0;
	bool hasDefaultAugmentParam = false;
	const bool bHasParamsOrReturnValue = (Function->Children != NULL);
	if (bHasParamsOrReturnValue)
	{
		int32 ParamIndex = 0;
		if (bIsStaticFunc)
			ParamIndex = -1;
// 		auto x = Function->GetName() == "PlaySoundAtLocation";
		for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
		{
			UProperty* Param = *ParamIt;
			if (Param->GetName() == "ReturnValue")
			{
				returnType = GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue);
			}
			else
			{
				++count_all_param;
				if (index_first_default == 0)
				{
					FString defaultvalue = Function->GetMetaData(*FString::Printf(TEXT("CPP_Default_%s"), *Param->GetName()));
					if (!defaultvalue.IsEmpty())
						index_first_default = count_all_param;
				}
			}
		}
		if (index_first_default != 0 )
			count_default_param = count_all_param - index_first_default + 1;

		Params += TEXT("\t#ifdef LuaDebug\r\n");
		Params += TEXT("\tint totalPamCount = lua_gettop(L);\r\n");
		if (bIsStaticFunc)
		{
			Params += FString::Printf(TEXT("\tif (totalPamCount < %d)\r\n"), count_all_param - count_default_param);
		}
		else
		{
			Params += FString::Printf(TEXT("\tif (totalPamCount < %d)\r\n"), count_all_param - count_default_param + 1);
		}
		Params += FString::Printf(TEXT("\t{\r\n\t\tlua_pushstring(L, \"too less param in %s\");\r\n"), *Function->GetName());
		Params += FString::Printf(TEXT("\t\tlua_error(L);\r\n\t\treturn 0;\r\n\t}\r\n"));
		Params += TEXT("\t#endif\r\n");

		if (count_default_param > 0)
		{
			Params += TEXT("\tint num_params = lua_gettop(L);\r\n");
			for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
			{
				UProperty* Param = *ParamIt;
				if (! (Param->GetName() == "ReturnValue"))
				{
					if (Param->IsA(UStructProperty::StaticClass()))
						Params += FString::Printf(TEXT("\t%s* %s;\r\n"), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue), *Param->GetName());
					else
						Params += FString::Printf(TEXT("\t%s %s;\r\n"), *GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue), *Param->GetName());
				}
			}
			if (!returnType.IsEmpty())
				Params += FString::Printf(TEXT("\t%s result;\r\n"), *returnType);
		}
		int count_init_param = 0;
		if (count_default_param > 0 && count_default_param + count_init_param >= count_all_param)
		{
			Params += CallCode(Function, bIsStaticFunc, !returnType.IsEmpty(), count_init_param, paramList, ClassNameCPP, count_init_param == count_all_param);
		}
		for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt, ++ParamIndex)
		{
			UProperty* Param = *ParamIt;
			if (Param->IsA(UArrayProperty::StaticClass()))
			{ 
				FString nameCpp = GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue);
				if (Param->GetName() != "ReturnValue")
				{
					paramList += Param->GetName() + ",";
					count_init_param++;
					if (count_default_param > 0 )
					{
						Params += InitializeParam(Param, ParamIndex, false, Param->GetName());
						if (count_default_param + count_init_param >= count_all_param)
							Params += CallCode(Function, bIsStaticFunc, !returnType.IsEmpty(), count_init_param, paramList, ClassNameCPP, count_init_param == count_all_param);
					}
					else
					{
						Params += FString::Printf(TEXT("\t%s %s;\r\n"), *nameCpp, *Param->GetName());
						Params += InitializeParam(Param, ParamIndex, false, Param->GetName());
					}
				}
			}
			else
			{
				if (Param->GetName() != "ReturnValue")
				{
					FString initParam = InitializeFunctionDispatchParam(Function, Param, ParamIndex);
					FString nameCpp = GetPropertyTypeCPP(Param, CPPF_ArgumentOrReturnValue);

					if (Param->IsA(UStructProperty::StaticClass()))
						paramList += "*" + Param->GetName() + ",";
					else
						paramList += Param->GetName() + ",";

					count_init_param++;
					if (count_default_param > 0)
					{
						if (!nameCpp.Contains("*") && Param->IsA(UStructProperty::StaticClass()))
							Params += FString::Printf(TEXT("\t%s = %s;\r\n"), *Param->GetName(), *initParam);
						else
							Params += FString::Printf(TEXT("\t%s = %s;\r\n"), *Param->GetName(), *initParam);
						// SpawnSoundAttached ambiouscall blame UE4 not me
						if (count_default_param + count_init_param >= count_all_param  && !(FuncName == "SpawnSoundAttached" && count_init_param == 4))
							Params += CallCode(Function, bIsStaticFunc, !returnType.IsEmpty(), count_init_param, paramList, ClassNameCPP, count_init_param == count_all_param);
					}
					else
					{
						if (!nameCpp.Contains("*") && Param->IsA(UStructProperty::StaticClass()))
							Params += FString::Printf(TEXT("\t%s* %s = %s;\r\n"), *nameCpp, *Param->GetName(), *initParam);
						else
							Params += FString::Printf(TEXT("\t%s %s = %s;\r\n"), *nameCpp, *Param->GetName(), *initParam);
					}
				}
			}
		}
		
	}
	if (count_default_param == 0)
	{
		if (!returnType.IsEmpty())
			Params += FString::Printf(TEXT("\t%s result = "), *returnType);
		else
			Params += "\t";
		FString callobj = "Obj->";
		if (bIsStaticFunc)
			callobj = FString::Printf(TEXT("%s::"), *ClassNameCPP);
		if (paramList.IsEmpty())
		{
			Params += FString::Printf(TEXT("%s%s();\r\n"), *callobj, *Function->GetName());
		}
		else
		{
			paramList.RemoveAt(paramList.Len() - 1);
			Params += FString::Printf(TEXT("%s%s(%s);\r\n"), *callobj, *Function->GetName(), *paramList);
		}
	}
	else
	{
		Params += "\tend:\r\n";
	}
	return Params;
}

FString FLuaScriptCodeGenerator::FuncCode(FString  ClassNameCPP, FString classname, UFunction* Function, UClass* FuncSuper)
{
	UProperty* ReturnValue = NULL;
	FString FunctionBody = GenerateWrapperFunctionDeclaration(ClassNameCPP, classname, Function);
	FunctionBody += TEXT("\r\n{\r\n");
	if (FuncSuper == NULL)
	{
		if (Function->FunctionFlags & FUNC_Public)
		{
			bool bIsStaticFunc = !!(Function->FunctionFlags & FUNC_Static);
			if (!bIsStaticFunc)
				FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP));
			FunctionBody += GenerateFunctionDispatch(Function, ClassNameCPP, bIsStaticFunc);

			int returnCount = 0;
			for (TFieldIterator<UProperty> ParamIt(Function); !ReturnValue && ParamIt; ++ParamIt)
			{
				UProperty* Param = *ParamIt;
				if (Param->GetPropertyFlags() & CPF_ReturnParm)
				{
					ReturnValue = Param;
					FString returnValueName = "result";
					FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *Push(ClassNameCPP, Function, ReturnValue, returnValueName));
					returnCount++;
				}
			}
			for (TFieldIterator<UProperty> ParamIt(Function); ParamIt; ++ParamIt)
			{
				UProperty* Param = *ParamIt;
				if ((Param->GetPropertyFlags() & (CPF_ConstParm | CPF_OutParm | CPF_ReturnParm)) == CPF_OutParm)
				{
					FString name = Param->GetName();
					FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *Push(ClassNameCPP, nullptr, Param, name));
					returnCount++;
				}
			}
			FunctionBody += FString::Printf(TEXT("\treturn %d;\r\n"), returnCount);
		}
		else
		{
			FunctionBody += FString::Printf(TEXT("\treturn 0;\r\n"));
		}
	}
	else
	{
		FunctionBody += FString::Printf(TEXT("\treturn %s_%s(L);\r\n"), *GetClassNameCPP(FuncSuper), *Function->GetName());
	}
	FunctionBody += TEXT("}\r\n\r\n");
	return FunctionBody;
}

FString FLuaScriptCodeGenerator::ExportFunction(const FString& ClassNameCPP, UClass* Class, UFunction* Function)
{
	UClass* FuncSuper = NULL;

	if (Function->GetOwnerClass() != Class)
	{
		// Find the base definition of the function
		if (ExportedClasses.Contains(Function->GetOwnerClass()->GetFName()))
		{
			FuncSuper = Function->GetOwnerClass();
		}
	}
	
	FString GeneratedGlue = FuncCode(ClassNameCPP, Class->GetName(), Function, FuncSuper);

	auto& Exports = ClassExportedFunctions.FindOrAdd(Class);
	Exports.Add(Function->GetFName());

	return GeneratedGlue;
}

bool FLuaScriptCodeGenerator::IsPropertyTypeSupported(UProperty* Property) const
{
	bool bSupported = true;
	if (Property->IsA(UStructProperty::StaticClass()))
	{
		UStructProperty* StructProp = CastChecked<UStructProperty>(Property);
		if (!isStructSupported(StructProp->Struct))
		{
			bSupported = false;
		}
	}
	else if (Property->IsA(UArrayProperty::StaticClass()))
	{
		bSupported = true;
	}
	else if (Property->IsA(ULazyObjectProperty::StaticClass()) ||
		Property->IsA(UAssetObjectProperty::StaticClass()) ||
// 		Property->IsA(UWeakObjectProperty::StaticClass()) ||
		Property->IsA(UAssetClassProperty::StaticClass()) 
		)
	{
		bSupported = false;
	}
	else if (Property->IsA(UByteProperty::StaticClass()))
		bSupported = true;
	else if (!Property->IsA(UIntProperty::StaticClass()) &&
		!Property->IsA(UFloatProperty::StaticClass()) &&
		!Property->IsA(UStrProperty::StaticClass()) &&
		!Property->IsA(UNameProperty::StaticClass()) &&
		!Property->IsA(UBoolProperty::StaticClass()) &&
		!Property->IsA(UObjectPropertyBase::StaticClass()) &&
		!Property->IsA(UClassProperty::StaticClass()))
	{
		bSupported = false;
	}

	return bSupported;
}

bool FLuaScriptCodeGenerator::CanExportProperty(const FString& ClassNameCPP, UClass* Class, UProperty* Property)
{
	if (Property->GetName() == "BookMarks")
		return false;

	if ((Property->PropertyFlags & CPF_Deprecated))
	{
		return false;
	}

	bool bsupport = FScriptCodeGeneratorBase::CanExportProperty(ClassNameCPP, Class, Property);
	if (bsupport)
	{
		if (auto p = Cast<UArrayProperty>(Property))
			if (GetPropertyType(p->Inner) == "" || !IsPropertyTypeSupported(p->Inner))
				return false;
		return IsPropertyTypeSupported(Property);
	}
	else
		return false;
}

FString FLuaScriptCodeGenerator::GetPropertyType(UProperty* Property) const
{
	if (Property->IsA(UStrProperty::StaticClass()))
	{
		return FString("UStrProperty");
	}
	else if (Property->IsA(UInt8Property::StaticClass()))
	{
		return FString("UInt8Property");
	}
	else if (Property->IsA(UIntProperty::StaticClass()))
	{
		return FString("UIntProperty");
	}
	else if (Property->IsA(UFloatProperty::StaticClass()))
	{
		return FString("UFloatProperty");
	}
	else if (Property->IsA(UBoolProperty::StaticClass()))
	{
		return FString("UBoolProperty");
	}
	else if (Property->IsA(UNameProperty::StaticClass()))
	{
		return FString("UNameProperty");
	}
	else if (Property->IsA(UTextProperty::StaticClass()))
	{
		return FString("UTextProperty");
	}
	else if (Property->IsA(UWeakObjectProperty::StaticClass()))
	{
		// return FString("");
		return FString("UWeakObjectProperty");
	}
	else if (Property->IsA(UObjectPropertyBase::StaticClass()))
	{
		return FString("UObjectPropertyBase");
	}
	else if (Property->IsA(UClassProperty::StaticClass()))
	{
		return FString("UClassProperty");
	}
	else if (Property->IsA(UStructProperty::StaticClass()))
	{
		return FString("UStructProperty");
	}
	else if (Property->IsA(UArrayProperty::StaticClass()))
	{
		return FString("UArrayProperty");
	}
	else
	{
		return FString("");
	}
}

FString FLuaScriptCodeGenerator::GetPropertyGetFunc(UProperty* Property) const
{
	if (Property->IsA(UClassProperty::StaticClass()))
	{
		return FString("ContainerPtrToValuePtr<void>");
	}
	if (Property->IsA(UWeakObjectProperty::StaticClass()))
	{
		return FString("GetObjectPropertyValue_InContainer");
	}
	else if (Property->IsA(UObjectPropertyBase::StaticClass()))
	{
		return FString("GetObjectPropertyValue_InContainer");
	}
	else if (Property->IsA(UStructProperty::StaticClass()))
	{
		return FString("ContainerPtrToValuePtr<uint8>");
	}
	else
	{
		return FString("GetPropertyValue_InContainer");
	}
}

FString FLuaScriptCodeGenerator::GetPropertySetFunc(UProperty* Property) const
{
	if (Property->IsA(UObjectPropertyBase::StaticClass()))
	{
		return FString("SetObjectPropertyValue_InContainer");
	}
	else if (Property->IsA(UStructProperty::StaticClass()))
	{
		return FString("CopyCompleteValue");
	}
	else
	{
		return FString("SetPropertyValue_InContainer");
	}
}
FString FLuaScriptCodeGenerator::GetPropertyCastType(UProperty* Property)
{
	if (Property->IsA(UClassProperty::StaticClass()))
	{
		return FString("UClass*");
	}
	else if (Property->IsA(UStructProperty::StaticClass()))
	{
		FString typenamecpp = GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue);
		return FString::Printf(TEXT("%s*"), *typenamecpp);
	}
	else if (Property->IsA(UWeakObjectProperty::StaticClass()))
	{
		GetWeakObjType(Property, objtype)
		return typeName + "*";
	}
	else
	{
		return FString("");
	}
}
FString FLuaScriptCodeGenerator::GetPropertySetCastType(UProperty* Property)
{
	if (Property->IsA(UBoolProperty::StaticClass()))
	{
		return FString("bool");
	}
	else if (Property->IsA(UStructProperty::StaticClass()))
	{
		FString typecpp = GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue);
		return FString::Printf(TEXT("%s*"), *typecpp);
	}
	else if (Property->IsA(UWeakObjectProperty::StaticClass()))
	{
		return GetPropertyCastType(Property);
	}
	else
	{
		return FString("");
	}
}
FString FLuaScriptCodeGenerator::GetterCode(FString ClassNameCPP, FString classname, FString FuncName, UProperty* Property, UClass* PropertySuper)
{
	FString GeneratedGlue = GenerateWrapperFunctionDeclaration(ClassNameCPP, classname, FuncName);
	GeneratedGlue += TEXT("\r\n{\r\n");
	FString FunctionBody;

	//FunctionBody += FString::Printf(TEXT("\tstatic UProperty* Property = FindScriptPropertyHelper(%s::StaticClass(), TEXT(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
	if (PropertySuper == nullptr)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP));
		if (Property->PropertyFlags & CPF_NativeAccessSpecifierPublic)
		{
			if (Property->ArrayDim>1)
			{
				FunctionBody += FString::Printf(TEXT("\tauto result = Obj->%s;\r\n"), *Property->GetName());
			}
			else if ( Property->IsA(UStructProperty::StaticClass()))
			{
				FunctionBody += FString::Printf(TEXT("\t%s& result = Obj->%s;\r\n"), *GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue), *Property->GetName());
			}
			else
			{
				FunctionBody += FString::Printf(TEXT("\t%s result = Obj->%s;\r\n"), *GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue), *Property->GetName());
			}
		}
		else
		{ 
			if( Property->ArrayDim <= 1 )
			{
				FString statictype = GetPropertyType(Property);
				if (!statictype.IsEmpty())
				{
					FunctionBody += FString::Printf(TEXT("\tUProperty* property = UTableUtil::GetPropertyByName(FString(\"%s\"), FString(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
					FunctionBody += FString::Printf(TEXT("\t%s* p = Cast<%s>(property);\r\n"), *statictype, *statictype);
					FString typecpp = GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue);
					FString typecast = GetPropertyCastType(Property);
					if (typecast.IsEmpty())
						typecast = typecpp;
					if (!typecpp.Contains("TArray") && !typecpp.Contains("TWeakObjectPtr"))
					{
						if (Property->IsA(UStructProperty::StaticClass()))
							FunctionBody += FString::Printf(TEXT("\t%s result = (%s)p->%s(Obj);\r\n"), *typecast, *typecast, *GetPropertyGetFunc(Property));
						else
							FunctionBody += FString::Printf(TEXT("\t%s result = (%s)p->%s(Obj);\r\n"), *typecpp, *typecast, *GetPropertyGetFunc(Property));
					}
					else if (!typecpp.Contains("TArray") && typecpp.Contains("TWeakObjectPtr"))
					{
						FunctionBody += FString::Printf(TEXT("\t%s result = (%s)p->%s(Obj);\r\n"), *typecpp, *typecast, *GetPropertyGetFunc(Property));
					}
					else
					{
						FunctionBody += FString::Printf(TEXT("\tFScriptArrayHelper_InContainer result(p, Obj);\r\n"));
					}
				}
				else
				{
					FunctionBody += FString::Printf(TEXT("\tint result = 0;\r\n"));
				}
			}
			else
			{
			}
		}

		if( Property->ArrayDim>1 && !(Property->PropertyFlags & CPF_NativeAccessSpecifierPublic))
			FunctionBody += FString::Printf(TEXT("\treturn 0;\r\n"));
		else if (Property->IsA(UStructProperty::StaticClass()) && Property->PropertyFlags & CPF_NativeAccessSpecifierPublic)
			FunctionBody += FString::Printf(TEXT("\t%s\r\n\treturn 1;\r\n"), *Push(ClassNameCPP, NULL, Property, FString("&result")));
 		else if (Property->IsA(UWeakObjectProperty::StaticClass()))
 		{
			FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateReturnValueHandler(ClassNameCPP, NULL, Property));
	 	}
		else
			FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateReturnValueHandler(ClassNameCPP, NULL, Property));
	}
	else
	{
		FunctionBody += FString::Printf(TEXT("\treturn %s_%s(L);\r\n"),  *GetClassNameCPP(PropertySuper), *FuncName);
	}
	GeneratedGlue += FunctionBody;
	GeneratedGlue += TEXT("}\r\n\r\n");
	return GeneratedGlue;
}

FString FLuaScriptCodeGenerator::SetterCode(FString ClassNameCPP, FString classname, FString FuncName, UProperty* Property, UClass* PropertySuper)
{
	FString GeneratedGlue = GenerateWrapperFunctionDeclaration(ClassNameCPP, classname, FuncName);
	GeneratedGlue += TEXT("\r\n{\r\n");
	FString FunctionBody;
	if (PropertySuper == NULL)
	{
		FunctionBody += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP));
		if (Property->ArrayDim <= 1) {
			if (Property->PropertyFlags & CPF_NativeAccessSpecifierPublic)
			{
				FString typecpp = GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue);
				auto exceptionOne = Property->GetName() == "AnimBlueprintGeneratedClass";
				if (exceptionOne)
					FunctionBody += TEXT("\tObj->AnimBlueprintGeneratedClass = (UAnimBlueprintGeneratedClass*)(UTableUtil::tousertype(\"UClass\", 2));\r\n");
				else if (!Property->IsA(UArrayProperty::StaticClass()))
				{
					if (Property->IsA(UStructProperty::StaticClass()))
						FunctionBody += FString::Printf(TEXT("\tObj->%s = *%s;\r\n"), *Property->GetName(), *InitializeParam(Property, 0));
					else
						FunctionBody += FString::Printf(TEXT("\tObj->%s = %s;\r\n"), *Property->GetName(), *InitializeParam(Property, 0));
				}
				else if (Property->IsA(UArrayProperty::StaticClass()))
				{
					auto PropertyArr = Cast<UArrayProperty>(Property);
					FunctionBody += FString::Printf(TEXT("\t%s& val = Obj->%s;\r\n"), *GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue), *Property->GetName());
					FunctionBody += InitializeParam(Property, 0, false, "val");
				}
			}
			else
			{
				bool bIsStruct = Property->IsA(UStructProperty::StaticClass());
				FString statictype = GetPropertyType(Property);
				if (!statictype.IsEmpty())
				{
					FunctionBody += FString::Printf(TEXT("\tUProperty* property = UTableUtil::GetPropertyByName(FString(\"%s\"), FString(\"%s\"));\r\n"), *ClassNameCPP, *Property->GetName());
					FunctionBody += FString::Printf(TEXT("\t%s* p = Cast<%s>(property);\r\n"), *statictype, *statictype);
					if (!Property->IsA(UArrayProperty::StaticClass()))
					{
						FString typecpp = GetPropertyTypeCPP(Property, CPPF_ArgumentOrReturnValue);
						FString typecast = GetPropertySetCastType(Property);
						if (typecast.IsEmpty())
							typecast = typecpp;
						FunctionBody += FString::Printf(TEXT("\t%s value = %s;\r\n"), *typecast, *InitializeParam(Property, 0, bIsStruct));
						if (Property->IsA(UObjectPropertyBase::StaticClass()))
							FunctionBody += FString::Printf(TEXT("\tp->%s(Obj, (UObject*)value);\r\n"), *GetPropertySetFunc(Property));
						else if (Property->IsA(UStructProperty::StaticClass()))
							FunctionBody += FString::Printf(TEXT("\tp->%s(p->ContainerPtrToValuePtr<void>(Obj), (void*)value);\r\n"), *GetPropertySetFunc(Property));
						else
							FunctionBody += FString::Printf(TEXT("\tp->%s(Obj, value);\r\n"), *GetPropertySetFunc(Property));
					}
					else //if (Property->IsA(UArrayProperty::StaticClass()))
					{
						auto PropertyArr = Cast<UArrayProperty>(Property);
						auto innerType = PropertyArr->Inner;
						bIsStruct = innerType->IsA(UStructProperty::StaticClass());
						FString typecpp = GetPropertyTypeCPP(innerType, CPPF_ArgumentOrReturnValue);
						FString typecast = GetPropertySetCastType(innerType);
						if (typecast.IsEmpty())
							typecast = typecpp;
						FString innerStaticType = GetPropertyType(innerType);
						FunctionBody += FString::Printf(TEXT("\t%s* innerProperty = Cast<%s>(p->Inner);\r\n"), *innerStaticType, *innerStaticType);
						FunctionBody += FString::Printf(TEXT("\tFScriptArrayHelper_InContainer result(p, Obj);\r\n"));
						FunctionBody += FString::Printf(TEXT("\tint32 len = lua_objlen(L, -1);\r\n"));
						FunctionBody += FString::Printf(TEXT("\tresult.Resize(len);\r\n"));
						FunctionBody += FString::Printf(TEXT("\tlua_pushnil(L);\r\n"));
						FunctionBody += FString::Printf(TEXT("\twhile (lua_next(L, -2) != 0) {\r\n"));
						FunctionBody += FString::Printf(TEXT("\t\tint32 i = lua_tointeger(L, -2)-1;\r\n"));
						FunctionBody += FString::Printf(TEXT("\t\t%s value = %s;\r\n"), *typecast, *InitializeParam(innerType, -3, bIsStruct));

						if (innerType->IsA(UObjectPropertyBase::StaticClass()))
							FunctionBody += FString::Printf(TEXT("\t\tinnerProperty->%s(result.GetRawPtr(i), (UObject*)value);\r\n"), *GetPropertySetFunc(innerType));
						else if (innerType->IsA(UStructProperty::StaticClass()))
							FunctionBody += FString::Printf(TEXT("\t\tinnerProperty->%s(innerProperty->ContainerPtrToValuePtr<void>(result.GetRawPtr(i)), (void*)value);\r\n"), *GetPropertySetFunc(innerType));
						else
							FunctionBody += FString::Printf(TEXT("\t\tinnerProperty->%s(result.GetRawPtr(i), value);\r\n"), *GetPropertySetFunc(innerType));
						FunctionBody += FString::Printf(TEXT("\t\tlua_pop(L, 1);\r\n\t}\r\n"));

					}
				}
			}
		}
		else if (Property->PropertyFlags&CPF_NativeAccessSpecifierPublic)
		{
			FunctionBody += FString::Printf(TEXT("\tauto& val = Obj->%s;\r\n"), *Property->GetName());
			FunctionBody += FString::Printf(TEXT("\tint32 len = CPP_ARRAY_DIM(%s,%s);\r\n"), *Property->GetName(), *ClassNameCPP);
			FunctionBody += FString::Printf(TEXT("\tfor(int i = 0; i < len; ++i){\r\n"));
			FunctionBody += FString::Printf(TEXT("\t\tlua_pushinteger(L, i+1);\r\n"));
			FunctionBody += FString::Printf(TEXT("\t\tlua_rawget(L, -2);\r\n"));
			FunctionBody += FString::Printf(TEXT("\t\tif (!lua_isnil(L, -1))\r\n"));
			if (Property->IsA(UStructProperty::StaticClass()))
				FunctionBody += FString::Printf(TEXT("\t\t\tval[i] = *%s;\r\n"), *InitializeParam(Property, -3, false));
			else	
				
				FunctionBody += FString::Printf(TEXT("\t\t\tval[i] = %s;\r\n"), *InitializeParam(Property, -3, false));
			FunctionBody += FString::Printf(TEXT("\t\tlua_pop(L, 1);\r\n"), *InitializeParam(Property, -3, false));
			FunctionBody += FString::Printf(TEXT("\t}\r\n"), *InitializeParam(Property, -3, false));
		}

		FunctionBody += TEXT("\treturn 0;\r\n");
	}
	else
	{
		FunctionBody = FString::Printf(TEXT("\treturn %s_%s(L);\r\n"), *GetClassNameCPP(PropertySuper), *FuncName);
	}
	GeneratedGlue += FunctionBody;
	GeneratedGlue += TEXT("}\r\n\r\n");
	return GeneratedGlue;
}
FString FLuaScriptCodeGenerator::ExportProperty(const FString& ClassNameCPP, UClass* Class, UProperty* Property, int32 PropertyIndex)
{
	FString PropertyName = Property->GetName();
	UProperty* ReturnValue = NULL;
	UClass* PropertySuper = NULL;
	FString GeneratedGlue;
	if (Property->GetOwnerClass() != Class)
	{
		// Find the base class where this property was defined
		if (ExportedClasses.Contains(Property->GetOwnerClass()->GetFName()))
		{
			PropertySuper = Property->GetOwnerClass();
		}
	}

	// Getter	
	FString GetterName = FString::Printf(TEXT("Get_%s"), *PropertyName);
	GeneratedGlue += GetterCode(ClassNameCPP, Class->GetName(), GetterName, Property, PropertySuper);

	// Store the name of this getter as well as the name of the wrapper function
	FPropertyAccessor Getter;
	Getter.AccessorName = GetterName;
	Getter.FunctionName = FString::Printf(TEXT("%s_%s"), *ClassNameCPP, *GetterName);
	auto& Exports = ClassExportedProperties.FindOrAdd(Class);
	Exports.Add(Getter);

	//Setter
	FString SetterName = FString::Printf(TEXT("Set_%s"), *PropertyName);
	GeneratedGlue += SetterCode(ClassNameCPP, Class->GetName(), SetterName, Property, PropertySuper);
	// Store the name of this setter as well as the name of the wrapper function
	FPropertyAccessor Setter;
	Setter.AccessorName = SetterName;
	Setter.FunctionName = FString::Printf(TEXT("%s_%s"), *ClassNameCPP, *SetterName);
	Exports.Add(Setter);
	return GeneratedGlue;
}

FString FLuaScriptCodeGenerator::ExportAdditionalClassGlue(const FString& ClassNameCPP, UClass* Class)
{
	FString GeneratedGlue;

	const FString ClassName = Class->GetName();

	// Constructor and destructor
	if (!(Class->GetClassFlags() & CLASS_Abstract))
	{
		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class->GetName(), TEXT("New"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += TEXT("\tUObject* Outer = (UObject*)UTableUtil::tousertype(\"UObject\", 1);\r\n");
		GeneratedGlue += TEXT("\tFName Name = FName(luaL_checkstring(L, 2));\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tUObject* Obj = NewObject<%s>(Outer, Name);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t}\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tUTableUtil::pushclass(\"%s\", (void*)Obj);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\treturn 1;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");

		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class->GetName(), TEXT("Destroy"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += FString::Printf(TEXT("\t%s\r\n"), *GenerateObjectDeclarationFromContext(ClassNameCPP));
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t}\r\n\treturn 0;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");

		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class->GetName(), TEXT("CreateDefaultSubobject"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += TEXT("\tUObject* Outer = (UObject*)UTableUtil::tousertype(\"UObject\", 1);\r\n");
		GeneratedGlue += TEXT("\tFName Name = FName(luaL_checkstring(L, 2));\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tUObject* Obj = Outer->CreateDefaultSubobject<%s>(Name);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\tif (Obj)\r\n\t{\r\n");
		GeneratedGlue += TEXT("\t}\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tUTableUtil::pushclass(\"%s\", (void*)Obj);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\treturn 1;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");

		GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class->GetName(), TEXT("FObjectFinder"));
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += FString::Printf(TEXT("\tvoid* Obj = (void*)UTableUtil::FObjectFinder(%s::StaticClass(), luaL_checkstring(L, 1));\r\n"), *ClassNameCPP);
		GeneratedGlue += FString::Printf(TEXT("\tUTableUtil::pushclass(\"%s\", Obj);\r\n"), *ClassNameCPP);
		GeneratedGlue += TEXT("\treturn 1;\r\n");
		GeneratedGlue += TEXT("}\r\n\r\n");
	}
	
	GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class->GetName(), TEXT("Cast"));
	GeneratedGlue += TEXT("\r\n{\r\n");
	GeneratedGlue += TEXT("\tUObject* from = (UObject*)UTableUtil::tousertype(\"UObject\", 1);\r\n");
	GeneratedGlue += FString::Printf(TEXT("\t%s* to = Cast<%s>(from);\r\n"), *ClassNameCPP, *ClassNameCPP);
	GeneratedGlue += FString::Printf(TEXT("\tUTableUtil::pushclass(\"%s\", (void*)to);\r\n"), *ClassNameCPP);
	GeneratedGlue += TEXT("\treturn 1;\r\n");
	GeneratedGlue += TEXT("}\r\n\r\n");
	// Class: Equivalent of StaticClass()
	GeneratedGlue += GenerateWrapperFunctionDeclaration(ClassNameCPP, Class->GetName(), TEXT("Class"));
	GeneratedGlue += TEXT("\r\n{\r\n");
	GeneratedGlue += FString::Printf(TEXT("\tUClass* Class = %s::StaticClass();\r\n"), *ClassNameCPP);
	GeneratedGlue += TEXT("\tUTableUtil::pushclass(\"UClass\", (void*)Class);\r\n");
	GeneratedGlue += TEXT("\treturn 1;\r\n");
	GeneratedGlue += TEXT("}\r\n\r\n");

	//Library
	GeneratedGlue += FString::Printf(TEXT("static const luaL_Reg %s_Lib[] =\r\n{\r\n"), *ClassName);
	if (!(Class->GetClassFlags() & CLASS_Abstract))
	{
		GeneratedGlue += FString::Printf(TEXT("\t{ \"New\", %s_New },\r\n"), *ClassNameCPP);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Destroy\", %s_Destroy },\r\n"), *ClassNameCPP);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"CreateDefaultSubobject\", %s_CreateDefaultSubobject },\r\n"), *ClassNameCPP);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"FObjectFinder\", %s_FObjectFinder },\r\n"), *ClassNameCPP);
	}

	GeneratedGlue += FString::Printf(TEXT("\t{ \"Cast\", %s_Cast },\r\n"), *ClassNameCPP);
	GeneratedGlue += FString::Printf(TEXT("\t{ \"Class\", %s_Class },\r\n"), *ClassNameCPP);
	auto FunctionExports = ClassExportedFunctions.Find(Class);
	if (FunctionExports)
	{
		for (auto& FunctionName : *FunctionExports)
		{
			GeneratedGlue += FString::Printf(TEXT("\t{ \"%s\", %s_%s },\r\n"), *FunctionName.ToString(), *ClassNameCPP, *FunctionName.ToString());
		}
	}
	auto PropertyExports = ClassExportedProperties.Find(Class);
	if (PropertyExports)
	{
		for (auto& Accessor : *PropertyExports)
		{
			GeneratedGlue += FString::Printf(TEXT("\t{ \"%s\", %s },\r\n"), *Accessor.AccessorName, *Accessor.FunctionName);
		}
	}
	GeneratedGlue += TEXT("\t{ NULL, NULL }\r\n};\r\n\r\n");

	return GeneratedGlue;
}

bool FLuaScriptCodeGenerator::isStructSupported(UScriptStruct* thestruct) const
{
	FString name = thestruct->GetName();
	if (SupportedStruct.Contains(name))
		return true;
	FString luameta = thestruct->GetMetaData(TEXT("Lua"));
	if(!luameta.IsEmpty())
		return true;
// 	if (thestruct->StructFlags & STRUCT_RequiredAPI)
// 		return true;
	return false;
}

void FLuaScriptCodeGenerator::ExportStruct()
{
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		FString name = *It->GetName();
		if (!isStructSupported(*It))
			continue;
		FString namecpp = "F" + name;
		StructNames.Add(namecpp);
		const FString ClassGlueFilename = GeneratedCodePath / (name + TEXT(".script.h"));
		AllScriptHeaders.Add(ClassGlueFilename);
		FString GeneratedGlue(TEXT("#pragma once\r\n\r\n"));
		int32 PropertyIndex = 0;
		TArray<FString> allPropertyName;
		if (!NoexportPropertyStruct.Contains(name))
		{
			for (TFieldIterator<UProperty> PropertyIt(*It/*, EFieldIteratorFlags::ExcludeSuper*/); PropertyIt; ++PropertyIt, ++PropertyIndex)
			{
				UProperty* Property = *PropertyIt;
				if (CanExportProperty(namecpp, nullptr, Property))
				{
					allPropertyName.Add(Property->GetName());
					FString GetterName = FString::Printf(TEXT("Get_%s"), *Property->GetName());
					GeneratedGlue += GetterCode(namecpp, namecpp, GetterName, Property);
					FString SetterName = FString::Printf(TEXT("Set_%s"), *Property->GetName());
					GeneratedGlue += SetterCode(namecpp, namecpp, SetterName, Property);
				}
			}
			for (TFieldIterator<UFunction> FuncIt(*It /*, EFieldIteratorFlags::ExcludeSuper*/); FuncIt; ++FuncIt)
			{
				UFunction* Function = *FuncIt;
				if (CanExportFunction(namecpp, nullptr, Function))
				{
					GeneratedGlue += FuncCode(namecpp, namecpp, Function);
				}
			}
		}
		FString addition = FString::Printf(TEXT("static int32 %s_New(lua_State* L)\r\n{\r\n"), *namecpp);
		addition += FString::Printf(TEXT("\t%s* Obj = new %s;\r\n"), *namecpp, *namecpp);
		addition += FString::Printf(TEXT("\tUTableUtil::pushclass(\"%s\", (void*)Obj, true);\r\n"), *namecpp);
		addition += FString::Printf(TEXT("\treturn 1;\r\n}\r\n\r\n"));

		addition += FString::Printf(TEXT("static int32 %s_Destroy(lua_State* L)\r\n{\r\n"), *namecpp);
		addition += FString::Printf(TEXT("\t%s* Obj = (%s*)UTableUtil::tousertype(\"%s\",1);\r\n"), *namecpp, *namecpp, *namecpp);
		addition += FString::Printf(TEXT("\tdelete Obj;\r\n"));
		addition += TEXT("\treturn 0;\r\n}\r\n\r\n");

		addition += FString::Printf(TEXT("static int32 %s_Copy(lua_State* L)\r\n{\r\n"), *namecpp);
		addition += FString::Printf(TEXT("\t%s* Obj = (%s*)UTableUtil::tousertype(\"%s\",1);\r\n"), *namecpp, *namecpp, *namecpp);
		addition += FString::Printf(TEXT("\tUTableUtil::pushclass(\"%s\", (void*)(new %s(*Obj)), true);\r\n"), *namecpp, *namecpp);
		addition += TEXT("\treturn 1;\r\n}\r\n\r\n");

		GeneratedGlue += addition;

		GeneratedGlue += FString::Printf(TEXT("static const luaL_Reg %s_Lib[] =\r\n{\r\n"), *namecpp);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"New\", %s_New },\r\n"), *namecpp);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Destroy\", %s_Destroy },\r\n"), *namecpp);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Copy\", %s_Copy },\r\n"), *namecpp);


		for (auto& PropertyName : allPropertyName)
		{
			GeneratedGlue += FString::Printf(TEXT("\t{ \"Get_%s\", %s_Get_%s },\r\n"), *PropertyName, *namecpp, *PropertyName);
			GeneratedGlue += FString::Printf(TEXT("\t{ \"Set_%s\", %s_Set_%s },\r\n"), *PropertyName, *namecpp, *PropertyName);
		}
		GeneratedGlue += TEXT("\t{ NULL, NULL }\r\n};\r\n");

		SaveHeaderIfChanged(ClassGlueFilename, GeneratedGlue);
	}
}

void FLuaScriptCodeGenerator::ExportEnum()
{
	const FString ClassGlueFilename = GeneratedCodePath / TEXT("allEnum.script.h");
	FString GeneratedGlue;
	GeneratedGlue += TEXT("struct EnumItem\r\n{\r\n");
	GeneratedGlue += TEXT("\tconst char* key;\r\n");
	GeneratedGlue += TEXT("\tconst int32 value;\r\n};\r\n");
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		UEnum* e = *It;
		FString name = e->GetName();
		EnumtNames.Add(name);
		GeneratedGlue += FString::Printf(TEXT("static const EnumItem %s_Enum[] =\r\n{\r\n"), *name);
		for (int32 i = 0; i < e->GetMaxEnumValue(); ++i)
		{
			GeneratedGlue += FString::Printf(TEXT("\t{ \"%s\", %d },\r\n"), *e->GetEnumName(i), i);
		}
		GeneratedGlue += TEXT("\t{ NULL, NULL }\r\n};\r\n\r\n");
	}
	AllScriptHeaders.Add(ClassGlueFilename);
	SaveHeaderIfChanged(ClassGlueFilename, GeneratedGlue);
}

void FLuaScriptCodeGenerator::ExportClass(UClass* Class, const FString& SourceHeaderFilename, const FString& GeneratedHeaderFilename, bool bHasChanged)
{
	//ExportEnum();
	if (!CanExportClass(Class))
	{
		return;
	}

	UE_LOG(LogScriptGenerator, Log, TEXT("Exporting class %s"), *Class->GetName());

	ExportedClasses.Add(Class->GetFName());
	LuaExportedClasses.Add(Class);
	AllSourceClassHeaders.Add(SourceHeaderFilename);

	const FString ClassGlueFilename = GetScriptHeaderForClass(Class);
	AllScriptHeaders.Add(ClassGlueFilename);

	const FString ClassNameCPP = GetClassNameCPP(Class);
	FString GeneratedGlue(TEXT("#pragma once\r\n\r\n"));
	// 	auto x = Class->GetFName() == "AnimInstance";
		// Export all functions
	for (TFieldIterator<UFunction> FuncIt(Class /*, EFieldIteratorFlags::ExcludeSuper*/); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (CanExportFunction(ClassNameCPP, Class, Function))
		{
			GeneratedGlue += ExportFunction(ClassNameCPP, Class, Function);
		}
	}

	// Export properties that are owned by this class
	int32 PropertyIndex = 0;
	for (TFieldIterator<UProperty> PropertyIt(Class /*, EFieldIteratorFlags::ExcludeSuper*/); PropertyIt; ++PropertyIt, ++PropertyIndex)
	{
		UProperty* Property = *PropertyIt;
		//auto x = Property->GetName() == "GunOffset";
		if (CanExportProperty(ClassNameCPP, Class, Property))
		{
			UE_LOG(LogScriptGenerator, Log, TEXT("  %s %s"), *Property->GetClass()->GetName(), *Property->GetName());
			GeneratedGlue += ExportProperty(ClassNameCPP, Class, Property, PropertyIndex);
		}
	}

	GeneratedGlue += ExportAdditionalClassGlue(ClassNameCPP, Class);

	SaveHeaderIfChanged(ClassGlueFilename, GeneratedGlue);
}

void FLuaScriptCodeGenerator::GenerateWeakClass()
{
	const FString ClassGlueFilename = GeneratedCodePath / TEXT("allweakclass.script.h");
	const FString TraitGlueFilename = GeneratedCodePath / TEXT("traitweakclass.h");
	FString GeneratedGlue;
	FString TraitGlue = TEXT("#pragma once\r\ntemplate<class T>\r\nclass traitweakclass{};\n");
	for (auto &name:WeakPtrClass)
	{
		FString className = "TWeakObjectPtr_" + name;
		GeneratedGlue += FString::Printf(TEXT("class %s{\r\npublic:\r\n"), *className);
		GeneratedGlue += FString::Printf(TEXT("\tTWeakObjectPtr<%s> value;\r\n"), *name);
		GeneratedGlue += FString::Printf(TEXT("\t%s(TWeakObjectPtr<%s> v):value(v){};\r\n"), *className, *name);
		GeneratedGlue += FString::Printf(TEXT("};\r\n"));

		GeneratedGlue += GenerateWrapperFunctionDeclaration(className, className, "Get");
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += FString::Printf(TEXT("\t%s* Obj = (%s*)UTableUtil::tousertype(\"%s\", 1);\r\n"), *className, *className, *className);
		GeneratedGlue += FString::Printf(TEXT("\tif(Obj->value.IsValid())\r\n"));
		GeneratedGlue += FString::Printf(TEXT("\t\tUTableUtil::pushclass(\"%s\", (void*)(Obj->value.Get()));\r\n"), *name);
		GeneratedGlue += FString::Printf(TEXT("\telse\r\n\t\tlua_pushnil(L);\r\n"), *name);
		GeneratedGlue += FString::Printf(TEXT("\treturn 1;\r\n"), *name);
		GeneratedGlue += TEXT("}\r\n");

		GeneratedGlue += GenerateWrapperFunctionDeclaration(className, className, "Destroy");
		GeneratedGlue += TEXT("\r\n{\r\n");
		GeneratedGlue += FString::Printf(TEXT("\t%s* Obj = (%s*)UTableUtil::tousertype(\"%s\", 1);\r\n"), *className, *className, *className);
		GeneratedGlue += FString::Printf(TEXT("\tif(Obj)\r\n"));
		GeneratedGlue += FString::Printf(TEXT("\t\tdelete Obj;\r\n"), *name);
		GeneratedGlue += FString::Printf(TEXT("\treturn 0;\r\n"), *name);
		GeneratedGlue += TEXT("}\r\n");

		GeneratedGlue += FString::Printf(TEXT("static const luaL_Reg %s_Lib[] =\r\n{\r\n"), *className);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Get\", %s_Get },\r\n"), *className);
		GeneratedGlue += FString::Printf(TEXT("\t{ \"Destroy\", %s_Destroy },\r\n"), *className);
		GeneratedGlue += FString::Printf(TEXT("\t{ NULL, NULL}\r\n};\r\n\r\n\r\n"));
		LuaExportedTMPClasses.Add(className);

		TraitGlue += FString::Printf(TEXT("class %s;\r\n"), *name);
		TraitGlue += FString::Printf(TEXT("class %s;\r\n"), *className);
		TraitGlue += FString::Printf(TEXT("template<>\r\nclass traitweakclass<%s>{\r\n"), *name);
		TraitGlue += FString::Printf(TEXT("public:\r\n\tusing traitType = %s;\r\n"), *className);
		TraitGlue += FString::Printf(TEXT("};\r\n\r\n"));
		// TraitGlue += FString::Printf(TEXT("\tstatic const char* name;\r\n};\r\n"), *className);
		// TraitGlue += FString::Printf(TEXT("const char* traitweakclass<%s>::name = \"%s\";\r\n\r\n"), *name, *className);

	}
	AllScriptHeaders.Insert(ClassGlueFilename, 0);
	SaveHeaderIfChanged(ClassGlueFilename, GeneratedGlue);
	SaveHeaderIfChanged(TraitGlueFilename, TraitGlue);
}

void FLuaScriptCodeGenerator::FinishExport()
{
	ExportEnum();
	ExportStruct();
	GenerateWeakClass();
	GlueAllGeneratedFiles();
	RenameTempFiles();
}

void FLuaScriptCodeGenerator::GlueAllGeneratedFiles()
{
	// Generate inl library file
	FString LibGlueFilename = GeneratedCodePath / TEXT("GeneratedScriptLibraries.inl");
	FString LibGlue;

	// Include all source header files
	for (auto& HeaderFilename : AllSourceClassHeaders)
	{
		// Re-base to make sure we're including the right files on a remote machine
		FString NewFilename(RebaseToBuildPath(HeaderFilename));
		LibGlue += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
	}

	// Include all script glue headers
	for (auto& HeaderFilename : AllScriptHeaders)
	{
		// Re-base to make sure we're including the right files on a remote machine
		FString NewFilename(FPaths::GetCleanFilename(HeaderFilename));
		LibGlue += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
	}

	LibGlue += TEXT("\r\nvoid LuaRegisterExportedClasses(lua_State* L)\r\n{\r\n");
	for (auto Class : LuaExportedClasses)
	{
		LibGlue += FString::Printf(TEXT("\tUTableUtil::loadlib(%s_Lib, \"%s\");\r\n"), *Class->GetName(), *GetClassNameCPP(Class));
	}
	for (auto Name : StructNames)
	{
		LibGlue += FString::Printf(TEXT("\tUTableUtil::loadlib(%s_Lib, \"%s\");\r\n"), *Name, *Name);
	}
	for (auto Name : EnumtNames)
	{
		LibGlue += FString::Printf(TEXT("\tUTableUtil::loadEnum(%s_Enum, \"%s\");\r\n"), *Name, *Name);
	}
	for (auto &Name : LuaExportedTMPClasses)
	{
		LibGlue += FString::Printf(TEXT("\tUTableUtil::loadlib(%s_Lib, \"%s\");\r\n"), *Name, *Name);
	}
	LibGlue += TEXT("}\r\n\r\n");

	SaveHeaderIfChanged(LibGlueFilename, LibGlue);
}
