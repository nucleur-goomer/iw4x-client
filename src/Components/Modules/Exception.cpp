#include "STDInclude.hpp"

// Stuff causes warnings
#pragma warning(push)
#pragma warning(disable: 4091)
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma warning(pop)

namespace Components
{
	Utils::Hook Exception::SetFilterHook;

	__declspec(noreturn) void Exception::ErrorLongJmp(jmp_buf _Buf, int _Value)
	{
		if (!*reinterpret_cast<DWORD*>(0x1AD7EB4))
		{
			TerminateProcess(GetCurrentProcess(), 1337);
		}

		longjmp(_Buf, _Value);
	}

	LONG WINAPI Exception::ExceptionFilter(LPEXCEPTION_POINTERS ExceptionInfo)
	{
		// Pass on harmless errors
		if (ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_INTEGER_OVERFLOW ||
			ExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_FLOAT_OVERFLOW)
		{
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		auto minidump = MinidumpUpload::CreateQueuedMinidump(ExceptionInfo);
		if (!minidump)
		{
			OutputDebugStringA("Failed to create new minidump!");
			Utils::OutputDebugLastError();
		}
		else
		{
			delete minidump;
		}

		if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
		{
			Logger::Error("Termination because of a stack overflow.\n");
		}
		else
		{
			Logger::Error("Fatal error (0x%08X) at 0x%08X.", ExceptionInfo->ExceptionRecord->ExceptionCode, ExceptionInfo->ExceptionRecord->ExceptionAddress);
		}

		//TerminateProcess(GetCurrentProcess(), ExceptionInfo->ExceptionRecord->ExceptionCode);

		return EXCEPTION_CONTINUE_SEARCH;
	}

	LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Exception::SetUnhandledExceptionFilterStub(LPTOP_LEVEL_EXCEPTION_FILTER)
	{
		Exception::SetFilterHook.Uninstall();
		LPTOP_LEVEL_EXCEPTION_FILTER retval = SetUnhandledExceptionFilter(&Exception::ExceptionFilter);
		Exception::SetFilterHook.Install();
		return retval;
	}

	LPTOP_LEVEL_EXCEPTION_FILTER Exception::Hook()
	{
		return SetUnhandledExceptionFilter(&Exception::ExceptionFilter);
	}

	Exception::Exception()
	{
#ifdef DEBUG
		// Display DEBUG branding, so we know we're on a debug build
		Renderer::OnFrame([]()
		{
			Game::Font* font = Game::R_RegisterFont("fonts/normalFont");
			float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

			// Change the color when attaching a debugger
			if (IsDebuggerPresent())
			{
				color[0] = 0.6588f;
				color[1] = 1.0000f;
				color[2] = 0.0000f;
			}

			Game::R_AddCmdDrawText("DEBUG-BUILD", 0x7FFFFFFF, font, 15.0f, 10.0f + Game::R_TextHeight(font), 1.0f, 1.0f, 0.0f, color, Game::ITEM_TEXTSTYLE_SHADOWED);
		});
#endif
#if !defined(DEBUG) || defined(FORCE_EXCEPTION_HANDLER)
		Exception::SetFilterHook.Initialize(SetUnhandledExceptionFilter, Exception::SetUnhandledExceptionFilterStub, HOOK_JUMP);
		Exception::SetFilterHook.Install();

		SetUnhandledExceptionFilter(&Exception::ExceptionFilter);
#endif

		//Utils::Hook(0x4B241F, Exception::ErrorLongJmp, HOOK_CALL).Install()->Quick();

		Command::Add("mapTest", [](Command::Params params)
		{
			std::string command;

			int max = (params.Length() >= 2 ? atoi(params[1]) : 16), current = 0;

			for (int i = 0;;)
			{
				char* mapname = Game::mapnames[i];
				if (!*mapname)
				{
					i = 0;
					continue;
				}

				if (!(i % 2)) command.append(fmt::sprintf("wait 250;disconnect;wait 750;", mapname)); // Test a disconnect
				else command.append(fmt::sprintf("wait 500;", mapname));                             // Test direct map switch
				command.append(fmt::sprintf("map %s;", mapname));

				++i, ++current;

				if (current >= max) break;
			}

			Command::Execute(command, false);
		});
		Command::Add("debug_exceptionhandler", [](Command::Params)
		{
			Logger::Print("Rerunning SetUnhandledExceptionHandler...\n");
			auto oldHandler = Exception::Hook();
			Logger::Print("Old exception handler was 0x%010X.\n", oldHandler);
		});

#pragma warning(push)
#pragma warning(disable:4740) // flow in or out of inline asm code suppresses global optimization
		Command::Add("debug_minidump", [](Command::Params)
		{
			// The following code was taken from VC++ 8.0 CRT (invarg.c: line 104)

			CONTEXT ContextRecord;
			EXCEPTION_RECORD ExceptionRecord;
			ZeroMemory(&ContextRecord, sizeof(CONTEXT));

			__asm 
			{
				mov [ContextRecord.Eax], eax
				mov [ContextRecord.Ecx], ecx
				mov [ContextRecord.Edx], edx
				mov [ContextRecord.Ebx], ebx
				mov [ContextRecord.Esi], esi
				mov [ContextRecord.Edi], edi
				mov word ptr [ContextRecord.SegSs], ss
				mov word ptr [ContextRecord.SegCs], cs
				mov word ptr [ContextRecord.SegDs], ds
				mov word ptr [ContextRecord.SegEs], es
				mov word ptr [ContextRecord.SegFs], fs
				mov word ptr [ContextRecord.SegGs], gs

				pushfd
				pop [ContextRecord.EFlags]
			}

			ContextRecord.ContextFlags = CONTEXT_CONTROL;
			ContextRecord.Eip = reinterpret_cast<DWORD>(_ReturnAddress());
			ContextRecord.Esp = reinterpret_cast<DWORD>(_AddressOfReturnAddress());
			ContextRecord.Ebp = *reinterpret_cast<DWORD*>(_AddressOfReturnAddress()) - 1;

			ZeroMemory(&ExceptionRecord, sizeof(EXCEPTION_RECORD));

			ExceptionRecord.ExceptionCode = EXCEPTION_BREAKPOINT;
			ExceptionRecord.ExceptionAddress = _ReturnAddress();

			EXCEPTION_POINTERS eptr;
			eptr.ExceptionRecord = &ExceptionRecord;
			eptr.ContextRecord = &ContextRecord;

			Exception::ExceptionFilter(&eptr);
		});
#pragma warning(pop)
	}

	Exception::~Exception()
	{
		Exception::SetFilterHook.Uninstall();
	}
}
