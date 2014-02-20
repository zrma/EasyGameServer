#include "stdafx.h"
#include "Exception.h"
#include <DbgHelp.h>


LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
	//////////////////////////////////////////////////////////////////////////
	// http://zrungee.tistory.com/56 참조
	// 디버깅 되고 있다면
	if ( IsDebuggerPresent() )
		return EXCEPTION_CONTINUE_SEARCH ;
	// http://www.moonistar.com/110 참조
	// 예외 처리 (except) 를 상위 구문으로 찾도록 요청 신호 반환
	//////////////////////////////////////////////////////////////////////////


	/// dump file 남기자.
	// 디버깅 중이 아닐 때 생긴 Crash에 대해서 dmp 파일을 남기는 부분
	HANDLE hFile = CreateFileA("EasyServer_minidump.dmp", GENERIC_READ | GENERIC_WRITE, 
		0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ) ; 

	if ( ( hFile != NULL ) && ( hFile != INVALID_HANDLE_VALUE ) ) 
	{
		MINIDUMP_EXCEPTION_INFORMATION mdei ; 
		//////////////////////////////////////////////////////////////////////////
		//	typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
		//		DWORD               ThreadId;
		//		PEXCEPTION_POINTERS ExceptionPointers;
		//		BOOL                ClientPointers;
		//	} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;
		//////////////////////////////////////////////////////////////////////////

		mdei.ThreadId           = GetCurrentThreadId() ; 
		mdei.ExceptionPointers  = exceptionInfo ; 
		mdei.ClientPointers     = FALSE ; 

		//////////////////////////////////////////////////////////////////////////
		// http://i5on9i.egloos.com/viewer/4839938 참조
		//
		// http://msdn.microsoft.com/en-us/library/ms680519.aspx 참조
		//////////////////////////////////////////////////////////////////////////

		MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory | 
			MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo | 
			MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules ) ; 
		//////////////////////////////////////////////////////////////////////////
		// MiniDumpWithPrivateReadWriteMemory
		//		Scan the virtual address space for PAGE_READWRITE memory to be included.
		//
		// MiniDumpWithDataSegs
		//		Include the data sections from all loaded modules.
		//		This results in the inclusion of global variables,
		//		which can make the minidump file significantly larger.
		//		For per-module control, use the ModuleWriteDataSeg enumeration value from MODULE_WRITE_FLAGS.
		//
		// MiniDumpWithHandleData
		//		Include high-level information about the operating system handles
		//		that are active when the minidump is made.
		//
		// MiniDumpWithFullMemoryInfo
		//		Include memory region information. For more information, see MINIDUMP_MEMORY_INFO_LIST.
		//		http://msdn.microsoft.com/en-us/library/ms680385.aspx
		//
		// MiniDumpWithThreadInfo
		//		Include thread state information. For more information, see MINIDUMP_THREAD_INFO_LIST.
		//		http://msdn.microsoft.com/en-us/library/ms680506.aspx
		//
		// MiniDumpWithUnloadedModules
		//		Include information from the list of modules that were recently unloaded,
		//		if this information is maintained by the operating system.
		//
		//		Windows Server 2003 and Windows XP:  The operating system does not maintain information 
		//		for unloaded modules until Windows Server 2003 with SP1 and Windows XP with SP2.
		//////////////////////////////////////////////////////////////////////////

		MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), 
			hFile, mdt, (exceptionInfo != 0) ? &mdei : 0, 0, NULL ) ; 
		//////////////////////////////////////////////////////////////////////////
		// http://blog.naver.com/goli81?Redirect=Log&logNo=10009869628 참조
		//
		//	MiniDumpWriteDump(
		//		_In_ HANDLE hProcess,
		//		_In_ DWORD ProcessId,
		//		_In_ HANDLE hFile,
		//		_In_ MINIDUMP_TYPE DumpType,
		//		_In_opt_ PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		//		_In_opt_ PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		//		_In_opt_ PMINIDUMP_CALLBACK_INFORMATION CallbackParam
		//	);
		//////////////////////////////////////////////////////////////////////////

		CloseHandle( hFile ) ; 

	}
	else 
	{
		printf("CreateFile failed. Error: %u \n", GetLastError()) ; 
	}

	return EXCEPTION_EXECUTE_HANDLER  ;
	// 예외처리 종료 신호 반환
}