#include "stdafx.h"
// 미리 컴파일 된 헤더

#include "EasyServer.h"
//////////////////////////////////////////////////////////////////////////
// 헤더 파일에 선언 되어 있는 것들
// 
//	enum ThreadType
//	{
//		THREAD_MAIN		= 1,
//		THREAD_CLIENT	= 2,
//		THREAD_DATABASE = 3
//	} ;
//
// extern __declspec(thread) int LThreadType ;
//
// unsigned int WINAPI ClientHandlingThread( LPVOID lpParam ) ;
// unsigned int WINAPI DatabaseHandlingThread( LPVOID lpParam ) ;
// void CALLBACK TimerProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue) ;
//////////////////////////////////////////////////////////////////////////

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
// 전역 변수로 매 타이밍마다 접속 허가 처리를 할 소켓 관리
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
__declspec(thread) int LThreadType = -1 ;
//////////////////////////////////////////////////////////////////////////
// TLS
// Thread Local Storage, 스레드마다 자신의 저장 공간을 가진다.
// 
// 각각의 스레드는 고유한 스택을 갖기 때문에 스택 변수(지역 변수)는 스레드 별로 고유하다.
// 각각의 스레드가 같은 함수를 실행한다고 해도 그 함수에서 정의된 지역변수는 실제로 
// 서로 다른 메모리 공간에 위치한다.
//
// 그러나 정적 변수나 전역 변수의 경우에는 프로세스 내의 모든 스레드에 의해서 공유됩니다.
//
// TLS는 정적, 전역 변수를 각각의 스레드에게 독립적으로 만들어 주고 싶을 때 사용한다.
// 분명히 같은 문맥(context)을 실행하고 있지만 실제로는 스레드 별로 다른 주소공간을 상대로 작업
//
// __declspec(thread) smgid_t call_smid;	// 이와 같이 선언
//////////////////////////////////////////////////////////////////////////
// 스레드 정체를 확인하기 위함
// 
// 메인 스레드인가?
// DB 핸들링 스레드인가?
// 클라이언트 핸들링 스레드인가?
// 
// 위와 같은 경우에 대해서 구분하기 위하여
// 헤더 파일에서 extern으로 선언 되어 있음 -> 다른 곳에서도 사용 하게 된다.
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
	// ExceptionFilter
	// Exception.cpp 쪽에서 전역 함수로 선언 되어 있음

	LThreadType = THREAD_MAIN ;

	//////////////////////////////////////////////////////////////////////////
	/// Manager Init
	GClientManager = new ClientManager() ;
	GDatabaseJobManager = new DatabaseJobManager() ;
	//////////////////////////////////////////////////////////////////////////
	// 각 매니저들은 각 헤더파일들에 extern 으로 선언 되어 있음
	// 여기서 new 해줌
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
	//////////////////////////////////////////////////////////////////////////
	// AF_INET : The Internet Protocol version 4 (IPv4) address family.
	// SOCK_STREAM : TCP
	//
	// socket protocol = 0
	// LAW SOCKET을 사용할 때 사용한다. 주로 사용 되지 않아 0이나 IPPROTO_TCP / IPPROTO_UPT로 사용 된다.
	//////////////////////////////////////////////////////////////////////////

	if (listenSocket == INVALID_SOCKET)
		return -1 ;

	int opt = 1 ;
	::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(int) ) ;
	//////////////////////////////////////////////////////////////////////////
	// 소켓의 세부 사항 조절 - getsockopt / setsockopt
	// http://vinchi.tistory.com/246 참조
	//
	// SOCKET	: 옵션 변경을 위한 소켓의 파일 디스크립터
	// level	: 변경 할 소켓 옵션의 레벨
	// optname	: 변경 할 옵션 이름
	// optval	: 변경 할 옵션 정보를 저장한 버퍼 포인터
	// optlen	: optval 버퍼 크기
	//
	// SOL_SOCKET 레벨에서 SO_REUSEADDR 사용 - 이미 사용 된 주소를 재사용(bind) 하도록 한다.
	// -> Address already in use 방지
	//
	// 참고
	// IPPROTO_TCP 레벨에서 사용 할 수 옵션과 데이터 형
	// TCP_NODELAY - 불값으로 nagle 알고리즘 제어
	//////////////////////////////////////////////////////////////////////////

	/// bind
	SOCKADDR_IN serveraddr ;
	ZeroMemory(&serveraddr, sizeof(serveraddr)) ;
	
	serveraddr.sin_family = AF_INET ;
	// IPv4

	serveraddr.sin_port = htons(LISTEN_PORT) ;
	// htons		= Hostshort TO uNsigned Short
	// LISTEN_PORT	= 9001

	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY) ;
	// INADDR_ANY를 사용하면 0.0.0.0 으로 설정하는 것과 같음
	//
	// 클라이언트에서 서버 주소를 어떤 것으로 접속을 시도하든 해당 서버의 지정 된 포트로 연결 시도하는
	// 접속에 대해서 모두 연결 허용하게 된다.
	//
	// SO_REUSEADDR 옵션을 사용하고, 동시에 두개의 서버를 실행하면
	// 오류가 나지 않고 한 TCP서버만 클라이언트 접속을 처리하게 되는데,
	// 
	// 여기에서 한 서버 프로그램은 INADDR_ANY 옵션을 주지않고 특정 IP주소로 바인딩 한 후
	// 두개의 서버를 실행하면 특정 주소로 바인딩 시킨 서버가 INADDR_ANY보다 우선권을 갖는다.


	//////////////////////////////////////////////////////////////////////////
	// 주소 정보를 담을 때 필요한 정보 3가지
	// 주소체계정보, IP주소, PORT번호
	//
	// 위의 3가지 정보를 저장해서 전달하기 위한 구조체가 sockaddr_in
	//
	//	struct sockaddr_in
	//	{
	//		sa_family_t		sin_family;		// 주소체계(AF_INET, AF_INET6, AF_LOCAL)
	//		uint16_t		sin_port;		// 16비트 TCP/UDP PORT 번호
	//		struct in_addr	sin_addr;		// 32비트 IP주소
	//		char			sin_zero[8];	// 사용되지 않음
	//	}
	//
	// 마지막 sin_zero는 구조체의 크기를 맞추기 위함
	//
	// 기본적으로 sockaddr_in 으로 정보를 얻어 sockaddr에 정보를 전달하는 구조로 되어있는데
	//
	// sockaddr의 구조를 살펴보면
	//
	//	struct sockaddr
	//	{
	//		sa_family_t		sin_family;		// 주소체계
	//		char			sa_data[14];	// 주소정보
	//	};
	//
	// 위와 같은데 전달 했을 때 정확한 결과를 얻기 위해서
	// 두 구조체의 크기를 같게 하기 위해 sin_zero를 추가 한 것
	//////////////////////////////////////////////////////////////////////////

	int ret = bind(listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) ;
	// socket() 함수로 소켓을 생성 이후에 소켓에 주소 값을 설정하는 함수가 bind()

	if (ret == SOCKET_ERROR)
		return -1 ;
	
	/// listen
	ret = listen(listenSocket, SOMAXCONN) ;
	// SOMAXCONN = BackLog size : 연결 요청 대기 큐의 사이즈
	if (ret == SOCKET_ERROR)
		return -1 ;

	/// auto-reset event
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL) ;
	//////////////////////////////////////////////////////////////////////////
	//	HANDLE CreateEvent( 
	//		LPSECURITY_ATTRIBUTES lpEventAttributes,	// 일반적으로 NULL, 보안 관련
	//		BOOL bManualReset,							// 수동(TRUE), 자동(FALSE)
	//		BOOL bInitialState,							// 생성시 이벤트가 적용된 상태인가
	//		LPTSTR lpName);								// 이벤트 네임
	//
	// 이벤트 - 스레드를 사용하면서 스레드를 죽이거나(Kill) 프로세스간 통신, 무언가 알릴 때 등 다양하게 사용
	// 
	// 두번째 파라미터를 수동으로 둘 경우 SetEvent()로 이벤트를 발생시킬 경우,
	// 수동으면 이벤트 발생후 ResetEvent()를 반드시 해주어야 이벤트가 다시 발생되고
	// 자동일 경우는 ResetEvent()를 안 해도 됨. 용도에 맞게 사용
	//
	// 위의 경우는 자동 리셋으로 설정되어 있음
	//////////////////////////////////////////////////////////////////////////

	if (hEvent == NULL)
	{
		WSACleanup() ;
		return -1 ;
	}

	/// I/O Thread
	DWORD dwThreadId ;
	HANDLE hThread = (HANDLE)_beginthreadex (NULL, 0, ClientHandlingThread, hEvent, 0, (unsigned int*)&dwThreadId) ;
	//////////////////////////////////////////////////////////////////////////
	// 클라이언트 핸들링 스레드 쪽에 이벤트를 전달하기 위해서 hEvent를 전달 인자로 대입
	//
	// http://blog.naver.com/hello20?Redirect=Log&logNo=150038699567
	//////////////////////////////////////////////////////////////////////////

    if (hThread == NULL)
	{
		CloseHandle( hEvent ) ;
		WSACleanup() ;
		return -1 ;
	}


	/// DB Thread
	HANDLE hDbThread = (HANDLE)_beginthreadex (NULL, 0, DatabaseHandlingThread, NULL, 0, (unsigned int*)&dwThreadId) ;
	if (hDbThread == NULL)
	{
		CloseHandle( hThread ) ;
		CloseHandle( hEvent ) ;
		WSACleanup() ;
		return -1 ;
	}

	/// accept loop
	while ( true )
	{
		g_AcceptedSocket = accept(listenSocket, NULL, NULL) ;
		// accept() = 연결 요청 대기 큐에서 대기 중인 클라이언트의 연결 요청을 수락하는 기능의 함수

		if ( g_AcceptedSocket == INVALID_SOCKET )
		{
			printf("accept: invalid socket\n") ;
			continue ;
		}

		/// accept event fire!
		//////////////////////////////////////////////////////////////////////////
		// 위에서 생성한 - 스레드 내부에 신호를 전달시키기 위한 hEvent 발생!
		//////////////////////////////////////////////////////////////////////////
		if ( !SetEvent(hEvent) )
		{
			printf("SetEvent error: %d\n", GetLastError()) ;
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
	// sqlite3_close

	if ( GClientManager != nullptr )
		delete GClientManager ;

	if ( GDatabaseJobManager != nullptr )
		delete GDatabaseJobManager ;

	return 0 ;
}

unsigned int WINAPI ClientHandlingThread( LPVOID lpParam )
{
	LThreadType = THREAD_CLIENT ;
	// TLS(Thread Local Storage)
	// 이 스레드는 클라이언트 핸들링 스레드

	HANDLE hEvent = (HANDLE)lpParam ;
	// 메인스레드에서 생성한 이벤트를 매개인자로 받아옴

	/// Timer
	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL) ;
	//////////////////////////////////////////////////////////////////////////
	//
	//	HANDLE WINAPI CreateWaitableTimer(
	//		_In_opt_  LPSECURITY_ATTRIBUTES lpTimerAttributes,		// 보안 설정
	//		_In_      BOOL bManualReset,							// 수동 리셋
	//		_In_opt_  LPCTSTR lpTimerName							// 타이머 네임
	//	);
	//
	// Sleep()을 사용한다면 Sleep 되어 있는 동안 해당 thread는 다른 일을 할 수 없다.
	// 즉, 이런 경우 Waitable Timer를 이용하여 UI쪽 thread는 지속적으로 Event를 받을 수 있도록 하여야 하고,
	// 별도의 작업 thread를 두어 그곳에서 Sleep을 해야 한다
	//
	// 그냥 Sleep을 할 경우 Wake-up 하는데 문제가 될 수 있으므로 Waitable Timer를 이용하는 것이 좋다.
	//
	// http://blog.naver.com/kjyong86/140151818181 참조
	//////////////////////////////////////////////////////////////////////////

	if (hTimer == NULL)
		return -1 ;
	
	LARGE_INTEGER liDueTime ;
	liDueTime.QuadPart = -10000000 ; // 1초 후부터 동작

	//////////////////////////////////////////////////////////////////////////
	// SetWaitabletimer 시간 단위 = 100 나노초
	// 1000,000,000나노초 = 1초
	//////////////////////////////////////////////////////////////////////////
	if ( !SetWaitableTimer(hTimer, &liDueTime, 100, TimerProc, NULL, TRUE) )
		return -1 ;
	//////////////////////////////////////////////////////////////////////////
	// 0.1초 주기로 TimerProc 함수 실행 하도록 콜백 설정
	// 
	// GClientManager->OnPeriodWork() ;
	// 클라이언트 매니저가 주기적으로 수행해야 할 일을 0.1초마다 수행한다.

	while ( true )
	{
		/// accept or IO/Timer completion 대기
		DWORD result = WaitForSingleObjectEx(hEvent, INFINITE, TRUE) ;
		// 이벤트 신호 들어올 때가지 무한 대기

		/// client connected
		if ( result == WAIT_OBJECT_0 )
		{
	
			/// 소켓 정보 구조체 할당과 초기화
			ClientSession* client = GClientManager->CreateClient(g_AcceptedSocket) ;
			//////////////////////////////////////////////////////////////////////////
			// 클라이언트 매니저에 매개인자로 접속 대기 큐에서 accept 된 소켓 정보를 넘겨 CreateClient
			// 클라 생성
			//
			// ClientManager.cpp 참조
			
			SOCKADDR_IN clientaddr ;
			int addrlen = sizeof(clientaddr) ;
			getpeername(g_AcceptedSocket, (SOCKADDR*)&clientaddr, &addrlen) ;
			// 소켓으로부터 클라이언트 네임(sockaddr 주소값)을 얻어옴

			// 클라 접속 처리
			if ( false == client->OnConnect(&clientaddr) )
			{
				client->Disconnect() ;
			}
			// ClientSession.cpp 참조
		
			continue ; // 다시 대기로
		}

		// APC에 있던 completion이 아니라면 에러다
		if ( result != WAIT_IO_COMPLETION )
			return -1 ;
		//////////////////////////////////////////////////////////////////////////
		// 이벤트 신호를 받아서 WaitForSingleObjectEx 하단으로 진입했는데
		// result 반환 값이 WAIT_IO_COMPLETION 가 아니라면 에러였음
		//////////////////////////////////////////////////////////////////////////
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
	// DB 쪽은 빠르게 무한루프 돌면서 처리

	return 0 ;
}

void CALLBACK TimerProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	assert( LThreadType == THREAD_CLIENT ) ;

	GClientManager->OnPeriodWork() ;
	// 클라이언트 쪽은 0.1초 단위로 콜백 함수 호출하면서 주기적으로 해야 할 일 처리
}
