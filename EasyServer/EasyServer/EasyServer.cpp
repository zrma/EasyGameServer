#include "stdafx.h"
#include "EasyServer.h"

#include "Config.h"
#include "..\..\PacketType.h"

#include "Exception.h"
#include "ClientSession.h"
#include "ClientManager.h"
#include "DatabaseJobManager.h"
#include "DbHelper.h"

//////////////////////////////////////////////////////////////////////////
#pragma comment(lib,"ws2_32.lib")
//////////////////////////////////////////////////////////////////////////
// 윈도우 소켓 - 이하 윈속은 기본적으로 두 가지를 설정해야 한다.
//
// 1. winsock2.h	인클루드
// 2. ws2_32.lib	라이브러리 링크
//
// stdafx.h 파일에
// #include <winsock2.h>
// 되어 있음
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
SOCKET g_AcceptedSocket = NULL ;
// 전역 변수로 이번 타이밍에 접속 허가 처리를 할 소켓 관리
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
__declspec(thread) int LThreadType = -1 ;
// 스레드 정체를 확인하기 위함
// 
// 메인 스레드인가?
// DB 핸들링 스레드인가?
// 클라이언트 핸들링 스레드인가?
// 
// 위와 같은 경우에 대해서 구분하기 위하여
//////////////////////////////////////////////////////////////////////////


int _tmain(int argc, _TCHAR* argv[])
{
	//////////////////////////////////////////////////////////////////////////
	// SetUnhandledExceptionFilter 
	// 예기치 않은 프로그램 시스템 예외를 받아낸다.
	//
	// 디버거보다 빠르게 예외(Exception)를 Catch하여 처리할 수 있음
	//////////////////////////////////////////////////////////////////////////

	/// crash 발생시 dump 남기기 위해서
	SetUnhandledExceptionFilter(ExceptionFilter) ;

	LThreadType = THREAD_MAIN ;

	//////////////////////////////////////////////////////////////////////////
	/// Manager Init
	GClientManager = new ClientManager() ;
	GDatabaseJobManager = new DatabaseJobManager() ;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	/// DB Helper 초기화
	if ( false == DbHelper::Initialize(DB_CONN_STR) )
		return -1 ;
	// 초기화에 실패 했다면 서버 종료
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	/// 윈속 초기화
	//////////////////////////////////////////////////////////////////////////
	WSADATA wsa ;
	if ( WSAStartup(MAKEWORD(2,2), &wsa) != 0 )
		return -1 ;
	// 윈도우 소켓 사용시 WSAStartup() 함수를 호출해야 winsock.dll을 이용 가능하다
	// 사용 시 유의점 : WSAStartup() / WSACleanup() 함수를 짝으로 맞춰 사용
	// 
	// 더미 클래스와 같은 구조체를 활용하여 생성자 / 소멸자에서 관리 할 수 있음
	//
	//	struct _WSADATA
	//	{
	//		WSADATA		wsaData
	//	public:
	//		_WSADATA()
	//		{
	//			WSAStartup(MAKEWORD(2,2), &wsa);
	//		}
	//		~_WSADATA()
	//		{
	//			WSACleanup();
	//		}
	//	} MyWSADATA;
	//
	// 그런데 이런 방식으로 활용하게 되면 위에 활용한 것과 같이 예외 처리 부분을 처리하지 못하게 됨
	//////////////////////////////////////////////////////////////////////////
	
	//////////////////////////////////////////////////////////////////////////
	// WSAStartup() 함수는 ws2_32.dll 안에 있는 함수들을 응용프로그램 영역으로 불러온다.
	// 
	// 더불어 로드한 dll 파일로부터, 사용할 수 있는 윈도우 소켓의 최상위 버전을 알아내거나
	// 어떤 버전의 소켓을 사용할 것인지 알려주는 역할도 함께 한다.
	//
	// WSAStartup() 함수의 호출이 실패 할 경우, 윈도우 소켓을 반드시 사용해야 하는 프로그램이라면
	// 어쩔 수 없이 프로그램을 종료해야 한다.
	//
	// 아니면 지원되지 않는 버전을 사용하겠다고 요청했다면, 하위 버전으로 호출이 성공 할 때 까지
	// 계속적으로 시도 할 수 있다.
	// 그러나 이 함수는 기본적으로 ws2_32.dll 파일을 사용하기 때문에, ws2_32.dll 파일이 반드시 존재해야 함
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// WSADATA 구조체
	//
	// WSAStartup() 함수가 반환하는 윈도우 소켓의 세부 정보의 저장에 사용 되는 구조체
	//////////////////////////////////////////////////////////////////////////

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0) ;
	if (listenSocket == INVALID_SOCKET)
		return -1 ;

	int opt = 1 ;
	::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int) ) ;

	/// bind
	SOCKADDR_IN serveraddr ;
	ZeroMemory(&serveraddr, sizeof(serveraddr)) ;
	serveraddr.sin_family = AF_INET ;
	serveraddr.sin_port = htons(LISTEN_PORT) ;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY) ;
	int ret = bind(listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) ;
	if (ret == SOCKET_ERROR)
		return -1 ;
	
	/// listen
	ret = listen(listenSocket, SOMAXCONN) ;
	if (ret == SOCKET_ERROR)
		return -1 ;

	/// auto-reset event
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL) ;
	if (hEvent == NULL)
		return -1 ;

	/// I/O Thread
	DWORD dwThreadId ;
	HANDLE hThread = (HANDLE)_beginthreadex (NULL, 0, ClientHandlingThread, hEvent, 0, (unsigned int*)&dwThreadId) ;
    if (hThread == NULL)
		return -1 ;


	/// DB Thread
	HANDLE hDbThread = (HANDLE)_beginthreadex (NULL, 0, DatabaseHandlingThread, NULL, 0, (unsigned int*)&dwThreadId) ;
	if (hDbThread == NULL)
		return -1 ;

	/// accept loop
	while ( true )
	{
		g_AcceptedSocket = accept(listenSocket, NULL, NULL) ;
		if ( g_AcceptedSocket == INVALID_SOCKET )
		{
			printf("accept: invalid socket\n") ;
			continue ;
		}

		/// accept event fire!
		if ( !SetEvent(hEvent) )
		{
			printf("SetEvent error: %d\n",GetLastError()) ;
			break ;
		}
	}

	CloseHandle( hThread ) ;
	CloseHandle( hEvent ) ;
	CloseHandle( hDbThread ) ;

	//////////////////////////////////////////////////////////////////////////
	// 윈속 종료
	WSACleanup() ;
	// 위에서 언급한 WSAStartup() 함수와 맞춰 사용한 WSACleanup()
	//////////////////////////////////////////////////////////////////////////

	DbHelper::Finalize() ;

	delete GClientManager ;
	delete GDatabaseJobManager ;
	return 0 ;
}

unsigned int WINAPI ClientHandlingThread( LPVOID lpParam )
{
	LThreadType = THREAD_CLIENT ;

	HANDLE hEvent = (HANDLE)lpParam ;

	/// Timer
	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL) ;
	if (hTimer == NULL)
		return -1 ;

	LARGE_INTEGER liDueTime ;
	liDueTime.QuadPart = -10000000 ; ///< 1초 후부터 동작
	if ( !SetWaitableTimer(hTimer, &liDueTime, 100, TimerProc, NULL, TRUE) )
		return -1 ;
		

	while ( true )
	{
		/// accept or IO/Timer completion   대기
		DWORD result = WaitForSingleObjectEx(hEvent, INFINITE, TRUE) ;

		/// client connected
		if ( result == WAIT_OBJECT_0 )
		{
	
			/// 소켓 정보 구조체 할당과 초기화
			
			ClientSession* client = GClientManager->CreateClient(g_AcceptedSocket) ;
			
			SOCKADDR_IN clientaddr ;
			int addrlen = sizeof(clientaddr) ;
			getpeername(g_AcceptedSocket, (SOCKADDR*)&clientaddr, &addrlen) ;

			// 클라 접속 처리
			if ( false == client->OnConnect(&clientaddr) )
			{
				client->Disconnect() ;
			}
		
			continue ; ///< 다시 대기로
		}

		// APC에 있던 completion이 아니라면 에러다
		if ( result != WAIT_IO_COMPLETION )
			return -1 ;
	}

	CloseHandle( hTimer ) ;
	return 0;
} 

unsigned int WINAPI DatabaseHandlingThread( LPVOID lpParam )
{
	LThreadType = THREAD_DATABASE ;

	while ( true )
	{
		/// 기본적으로 polling 하면서 Job이 있다면 처리 하는 방식
		GDatabaseJobManager->ExecuteDatabaseJobs() ;

		Sleep(1) ;
	}

	return 0 ;
}

void CALLBACK TimerProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	assert( LThreadType == THREAD_CLIENT ) ;

	GClientManager->OnPeriodWork() ;
}
