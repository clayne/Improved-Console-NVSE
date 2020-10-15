#include "command_printer.h"

#include <iomanip>
#include <sstream>
#include "declarations.h"
#include "console_arrays.h"
#include "console_variables.h"
#include "SafeWrite.h"
#include <cmath>

using namespace ImprovedConsole;

double g_lastResultValue = 0.0;
CommandReturnType g_lastReturnType = kRetnType_Ambiguous;
bool g_commandWasFunction = false;
bool g_lastCommandWasSet = false;
int g_commandPrints = 0;
bool g_cmdCalled = false;

std::string RetnTypeToString(CommandReturnType type)
{
	switch (type)
	{
	case kRetnType_Default: return "Float";
	case kRetnType_Form: return "Form";
	case kRetnType_String: return "String";
	case kRetnType_Array: return "Array";
	}
	return "Unknown type";
}

std::string FormatElement(NVSEArrayVarInterface::Element* element) {
	std::stringstream stream;
	
	switch (element->GetType())
	{
	case ConsoleArrays::kDataType_Numeric:
	{
		stream << std::fixed << std::setprecision(2) << element->Number();
		return stream.str();
	}
	case ConsoleArrays::kDataType_Form:
	{
		auto form = element->Form();
		const char* fullName = GetFullName(form);
		const char* editorId = form->GetName();
		UInt32 refId = form->refID;

		stream << "(FormID: " << std::hex << form->refID;
		if (strlen(editorId) > 0) {
			stream << ", EditorID: " << std::string(editorId);
		}

		if (strlen(fullName) > 0) {
			stream << ", Name: " << std::string(fullName);
		}
		stream << ")";
		return stream.str();
	}
	case ConsoleArrays::kDataType_String:
		stream << "\"" << element->String() << "\"";
		return stream.str();
	case ConsoleArrays::kDataType_Array:
		stream << "Array (ID " << (UInt32)element->Array() << ")";
		return stream.str();
	}
	return "Invalid";

}

void PrintArray(NVSEArrayVar* arrayPtr)
{
	if (arrayPtr == nullptr) {
		Console_Print("Invalid array");
		return;
	}

	const int size = GetArraySize(arrayPtr);
	if (size == 0) {
		Console_Print("Empty array");
		return;
	}

	std::vector<NVSEArrayVarInterface::Element> keys(size);
	std::vector<NVSEArrayVarInterface::Element> values(size);
	GetElements(arrayPtr, values.data(), keys.data());
	for (int i = 0; i < size; i++) {
		NVSEArrayVarInterface::Element key = keys[i];
		NVSEArrayVarInterface::Element value = values[i];

		auto keyStr = FormatElement(&key);
		auto valStr = FormatElement(&value);

		Console_Print("(Key) >> %s; (Value) >> %s", keyStr.c_str(), valStr.c_str());
	}
	Console_Print("Array Size >> %d", size);
}

void PrintArray(UInt32 arrayId)
{
	NVSEArrayVarInterface::Array* arrayPtr = LookupArrayByID(arrayId);
	PrintArray(arrayPtr);
}

void PrintForm(TESForm* form)
{
	if (!form) {
		Console_Print("Invalid Form");
		return;
	}

	Console_Print("(Form ID) >> %X", form->refID);

	const auto* editorId = form->GetName();
	const auto* fullName = GetFullName(form);
	if (strlen(editorId) > 0) {
		Console_Print("(Editor ID) >> %s", editorId);
	}
	if (strlen(fullName) > 0 && strcmp(fullName, "<no name>") != 0) {
		Console_Print("(Name) >> %s", fullName);
	}
}

void PrintForm(const UInt32 formId)
{
	TESForm* form = LookupFormByID(formId);
	PrintForm(form);
}

void PrintFloat(double value)
{
	Console_Print("(Float) >> %.2f", value);
}

void PrintString(const char* stringVar)
{
	Console_Print("(String) >> \"%s\"", stringVar);
}

void PrintUnknown(double value)
{
	const auto formId = *reinterpret_cast<UInt32*>(&value);
	Console_Print("(Unknown) >> %.2f / %d", value, formId);
}

void PrintVar(double value, CommandReturnType type)
{
	switch (type)
	{
	case kRetnType_Default:
	{
		PrintFloat(value);
		break;
	}
	case kRetnType_Form:
	{
		const auto formId = *reinterpret_cast<UInt32*>(&value);
		PrintForm(formId);
		break;
	}
	case kRetnType_String:
	{
		const auto* stringVar = GetStringVar(static_cast<UInt32>(value));
		PrintString(stringVar);
		break;
	}
	case kRetnType_Array:
	{
		const auto arrayId = static_cast<UInt32>(value);
		PrintArray(arrayId);
		break;
	}
	case kRetnType_ArrayIndex:
	case kRetnType_Ambiguous:
	case kRetnType_Max:
	default:
		PrintUnknown(value);
		break;
	}
}

void PrintElement(NVSEArrayElement& element)
{
	switch (static_cast<ConsoleArrays::ElementType>(element.GetType())) {
	case ConsoleArrays::kDataType_Numeric:
	{
		PrintFloat(element.Number());
		break;
	}
	case ConsoleArrays::kDataType_Form:
	{
		PrintForm(element.Form());
		break;
	}
	case ConsoleArrays::kDataType_String:
	{
		PrintString(element.String());
		break;
	}
	case ConsoleArrays::kDataType_Array:
	{
		PrintArray(element.Array());
		break;
	}
	case ConsoleArrays::kDataType_Invalid:
	default:
	{
		Console_Print("Invalid element");
		break;
	}
	}
}

bool PrintArrayIndex(std::string& varName, std::string& index)
{
	try
	{
		auto elem = ConsoleArrays::GetElementAtIndex(varName, index);
		PrintElement(elem);
		return true;
	}
	catch (std::invalid_argument& e)
	{
		PrintLog(e.what());
		return false;
	}
}

void PrintResult(CommandInfo* commandInfo, double result)
{
	auto returnType = static_cast<CommandReturnType>(GetCmdReturnType(commandInfo));

	g_commandWasFunction = true;
	g_lastReturnType = returnType;
	g_lastCommandWasSet = false;
	g_lastResultValue = result;

	if (g_commandPrints != 0)
	{
		PrintLog("something already printed, aborting");
		return;
	}
	if (std::isnan(result))
	{
		PrintLog("command did not change result value, aborting");
		return;
	}

	const auto formId = *reinterpret_cast<UInt32*>(&result);
	if (LookupFormByID(formId)) 
	{
		returnType = kRetnType_Form;
	}

	Console_Print("<Improved Console> %s", commandInfo->longName);
	PrintVar(result, returnType);
}

void __stdcall HookHandleVariableChanges(ScriptEventList *eventList)
{
	for (const auto& consoleVarEntry : g_consoleVarIdMap)
	{
		const auto consoleVar = consoleVarEntry.second;
		const auto* eventListVar = eventList->GetVariable(consoleVar->id);
		if (consoleVar->value != eventListVar->data)
		{
			consoleVar->value = eventListVar->data;
		}
	}
}

void __stdcall PrintCommandResult(double commandResult, ScriptRunner* scriptRunner, CommandInfo* commandInfo)
{
	if (commandInfo)
		PrintResult(commandInfo, commandResult);
	g_cmdCalled = false;
}

__declspec(naked) void PrintCommandResultHook()
{
	__asm
	{
		test al, al
		jz epilogue
		push eax // save return value
		call IsConsoleMode
		test al, al
		jz restoreReturnValue
		mov al, g_cmdCalled
		test al, al
		jz restoreReturnValue
		
		mov ecx, [ebp - 0x30] // CommandInfo*
		push ecx
		mov ecx, [ebp - 0xED0] // ScriptRunner*
		push ecx
		sub esp, 8
		fld qword ptr [ebp - 0xEC4]
		fstp qword ptr [esp] // double result
		call PrintCommandResult
	restoreReturnValue:
		pop eax
	epilogue:
		mov esp, ebp
		pop ebp
		ret 0x24
	}
}

__declspec(naked) void HookConsolePrint()
{
	__asm
	{
		inc g_commandPrints
		ret 0x8
	}
}

void __stdcall PreCommandCall(double* commandResult)
{
	// reset command prints so that when this gets incremented in hook we know that the function printed something
	g_commandPrints = 0;

	// set *result to NaN to compare with later so that we know if it got modified in the command or not
	*commandResult = std::numeric_limits<double>::quiet_NaN();

	// confirm function call subroutine was called
	g_cmdCalled = true;
}

__declspec(naked) void PreCommandCallHook()
{
	const static UInt32 returnAddress = 0x5E22F3;
 	__asm
	{
		// original assembly
		mov [ebp - 0xEB8], eax

		// hook
		lea eax, [ebp - 0xEC4]
		push eax
		call PreCommandCall
		
		jmp returnAddress
	}
}

void PatchPrintAnything() {
	WriteRelJump(0x5E239C, UInt32(PrintCommandResultHook));
	WriteRelJump(0x71D376, UInt32(HookConsolePrint));
	WriteRelJump(0x5E22ED, UInt32(PreCommandCallHook));
}

// TODO PrintCommandResultHook is not at a safe place (COC goodsprings)