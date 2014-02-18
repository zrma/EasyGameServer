#include "stdafx.h"
#include "ClientSession.h"
#include "..\..\PacketType.h"
#include "ClientManager.h"
#include "DatabaseJobContext.h"
#include "DatabaseJobManager.h"

//////////////////////////////////////////////////////////////////////////
// EasyServer.cpp 에서 클라이언트 매니저에서 CreateClient 한 후에
// 소켓 객체로부터 getpeername()를 이용해 주소 값을 뽑아 와서 OnConnect() 호출
//////////////////////////////////////////////////////////////////////////
bool ClientSession::OnConnect(SOCKADDR_IN* addr)
{
	//////////////////////////////////////////////////////////////////////////
	// 현재 이 부분은 OnConeect()가 호출 된 상황에서 살펴보면
	//
	// EasyServer.cpp의 ClientHandlingThread()에서 호출 한,
	// 클라이언트 매니저의 CreateClient()에서 new 해서 동적 할당 한,
	// 접속자(클라이언트) 하나 당 할당 된 객체 하나이다
	// 
	// 해당 객체에, 위의 인자로부터(EasyServer.cpp의 클라이언트 핸들링 스레드에서) 받은
	// 주소 값을 
	memcpy(&mClientAddr, addr, sizeof(SOCKADDR_IN)) ;

	/// 소켓을 넌블러킹으로 바꾸고
	u_long arg = 1 ;
	::ioctlsocket(mSocket, FIONBIO, &arg) ;
	//////////////////////////////////////////////////////////////////////////
	// ioctlsocket : 소켓 입출력 모드 제어 함수
	// 
	// FIONBIO
	// arg 매개변수가 0이 아닐 경우 넌블러킹 모드 활성화
	// arg 매개변수가 0일 경우 넌블러킹 모드 비활성화
	// arg 매개변수는 unsigned long 포인터.
	// 
	// 소켓이 생성되었을 때는 기본적으로 블로킹(동기) 모드로 동작한다는 사실 주의!
	// http://blog.naver.com/mansang_net/60005864500 참조
	//
	// 동기 통신 관련 설정은 FIOASYNC
	//////////////////////////////////////////////////////////////////////////

	/// nagle 알고리즘 끄기
	int opt = 1 ;
	::setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)) ;
	//////////////////////////////////////////////////////////////////////////
	// nagle 알고리즘
	// 패킷을 일정량이 찰 때 까지 모아서 한 번에 보냄으로써 네트워크 과부하 줄임
	// 데이터가 모일 때 까지 기다리므로 딜레이(지연)가 발생하게 됨
	// http://blog.naver.com/dreamfox/30041946114 참조
	//
	// setsockopt 옵션 및 데이터 형에 대한 주석은 EasyServer.cpp에서
	// SO_REUSEADDR 로 재사용 bind 하도록 설정 한 부분 주석을 참고 할 것
	//////////////////////////////////////////////////////////////////////////

	printf("[DEBUG] Client Connected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port)) ;
	
	mConnected = true ;

	return PostRecv() ;
}

bool ClientSession::PostRecv()
{
	if ( !IsConnected() )
		return false ;

	DWORD recvbytes = 0 ;
	DWORD flags = 0 ;
	WSABUF buf ;

	//////////////////////////////////////////////////////////////////////////
	// CircularBuffer.cpp 참조
	buf.len = (ULONG)mRecvBuffer.GetFreeSpaceSize() ;
	buf.buf = (char*)mRecvBuffer.GetBuffer() ;

	memset(&mOverlappedRecv, 0, sizeof(OverlappedIO)) ;
	//////////////////////////////////////////////////////////////////////////
	//	struct OverlappedIO : public OVERLAPPED
	//	{
	//		OverlappedIO() :mObject(nullptr)
	//		{}
	//
	//		ClientSession* mObject ;
	//	} ;
	//////////////////////////////////////////////////////////////////////////

	mOverlappedRecv.mObject = this ;
	//////////////////////////////////////////////////////////////////////////
	// 비동기 입력 호출이 완료 되었을 때 콜백 함수가 실행 되면
	// 어느 호출이 부른 것인지를 식별하기 위해
	// ClientSession* 를 담고 있는 멤버변수 mObject에 this (현재 객체) 주소 대입
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// 비동기 입출력 시작
	// 
	// Overlapped IO
	// 비동기 입출력 모델의 한 종류로 입출력 과정을 중첩 시킬 수 있다.
	// 실행한 입출력 과정이 종료되었다는 사실을 Event 오브젝트 혹은 Callback 함수로 통보 받는다. 
	// Completion Routines - CallBack 함수
	// http://hyulim.tistory.com/74
	// http://hyulim.tistory.com/73
	// http://blog.naver.com/smuoon4680/50141782462 참조
	//////////////////////////////////////////////////////////////////////////
	if ( SOCKET_ERROR == WSARecv(mSocket, &buf, 1, &recvbytes, &flags, &mOverlappedRecv, RecvCompletion) )
	//////////////////////////////////////////////////////////////////////////
	// WSARecv 인자 차례대로
	//
	// mSocket			: Overlapped IO 속성이 부여 된 소켓 핸들
	// &buf				: 수신 데이터 정보가 저장 될 버퍼(mOverlappedRecv)의 정보를 저장할 WSABUF 구조체의 주소
	// 1				: 위의 구조체의 크기
	// &recvbytes		: 수신 데이터 크기값 저장 할 변수의 주소
	// &flag			: 전송 특성 관련 플래그를 지정하거나 읽어옴
	// &mOverlappedRecv	: 수신 데이터 정보가 저장 될 버퍼 주소
	// RecvCompletion	: 비동기 통신 완료 되면 실행 할 콜백 함수 주소
	//					  하단에 RecvCompletion이라는 콜백 함수 구현 되어 있음. 참고!
	//////////////////////////////////////////////////////////////////////////
	{
		if ( WSAGetLastError() != WSA_IO_PENDING )
		//////////////////////////////////////////////////////////////////////////
		// WSA_IO_PENDING 에러에 대해서 
		// Recv(수신) 작업을 할때에는 GetQueuedCompletionStatus()로 비동기 작업 완료에 대한
		// 통지가 왔을 때 처리해 주면 된다.
		//////////////////////////////////////////////////////////////////////////
			return false ;
	}

	//////////////////////////////////////////////////////////////////////////
	IncOverlappedRequest() ;
	// Overlapped IO 요청 했음. 카운트 증가

	return true ;
}

void ClientSession::Disconnect()
{
	if ( !IsConnected() )
		return ;

	printf("[DEBUG] Client Disconnected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port)) ;

	::shutdown(mSocket, SD_BOTH) ;
	//////////////////////////////////////////////////////////////////////////
	// 소켓을 닫기 전 send, receive를 막기 위해 입출력 스트림을 종료시킨다.
	//
	// 우아한 연결 종료 half-close : 송, 수신 중 하나의 스트림만 종료해야하는 상황이 발생할 수 있다.
	// int shutdown( SOCKET s, int how ) 함수 사용
	// SD_RECEIVE : 입력( 수신 ) 스트림 종료
	// SD_SEND : 출력( 송신 ) 스트림 종료
	// SD_BOTH : 입, 출력 스트림 종료
	//
	// http://hyulim.tistory.com/68
	// http://network-dev.tistory.com/670
	// http://blog.naver.com/ksg7514/100163778831 참조
	//////////////////////////////////////////////////////////////////////////

	::closesocket(mSocket) ;

	mConnected = false ;
}


//////////////////////////////////////////////////////////////////////////
// 비동기 입력 완료 후 RecvCompletion 콜백 발생하면
// 받은 데이터 사이즈를 인자로 넘겨서 OnRead 실행
//////////////////////////////////////////////////////////////////////////
void ClientSession::OnRead(size_t len)
{
	// CircularBuffer.cpp 참조
	mRecvBuffer.Commit(len) ;
	// 버퍼 쓰기 영역 크기 증가

	/// 패킷 파싱하고 처리
	while ( true )
	{
		/// 패킷 헤더 크기 만큼 읽어와보기
		PacketHeader header ;
		if ( false == mRecvBuffer.Peek((char*)&header, sizeof(PacketHeader)) )
			return ;
		// 아직 버퍼가 패킷 헤더 사이즈만큼 차지 않았다.
		// 즉 헤더 부분만큼 마저도 읽기 실패

		/// 패킷 완성이 되는가? 
		if ( mRecvBuffer.GetStoredSize() < header.mSize )
			return ;
		//////////////////////////////////////////////////////////////////////////
		// 헤더에 담겨 있는 mSize는 패킷 전체 사이즈
		// 패킷 사이즈 = 패킷 헤더 + 페이로드(payload = 실제 데이터)
		// 
		// 버퍼 안에 패킷 사이즈 이상의 데이터가 담겨 있어야 꺼내서 핸들링 할 수 있음
		//////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// 이 위의 상단들에서 return 하게 되면 하단의 RecvCompletion 에서
		// 다시 PostRecv() 메소드를 호출하게 되어 통신이 재개 되어 다시 받아오고
		// 결국 요건을 충족 시키게 되면 아래로 진입 하게 되어 패킷 처리 하게 됨
		//
		// 패킷을 처리 하고 나면 또 위의 if 조건문들에 걸려서 return 되고
		// 다시 PostRecv() 메소드를 호출하게 되거나, 하단의 패킷 핸들링 switch문으로
		// 다시 진입하게 되거나... 이 일련 과정을 계속 반복
		//////////////////////////////////////////////////////////////////////////
		
		//////////////////////////////////////////////////////////////////////////
		// EasyServer.cpp의 클라이언트 핸들링 스레드에서 새로운 접속이 허용 되면
		// 
		// ClientSession* client = GClientManager->CreateClient(g_AcceptedSocket) ;
		// client->OnConnect(&clientaddr) ;
		//
		// CreateClient() 한 후에 ClientSession::OnConnect() 하면
		// OnConnect()에선 PostRecv() 하게 된다.
		//
		// PostRecv()에서 비동기 입력 종료 되면 콜백 RecvCompletion() 호출
		// 해당 함수 RecvCompletion()은 다시 PostRecv() 호출
		//
		//
		// 개요
		//
		// Create -> OnConnect -> PostRecv -> Callback -> PostRecv -> Callback -> ... 계속 반복
		//
		// (Callback에서 짬짬히 계속 OnRead하고, OnRead에서는 패킷이 완성 될 때마다 처리)
		//////////////////////////////////////////////////////////////////////////

		/// 패킷 핸들링
		switch ( header.mType )
		{
		case PKT_CS_LOGIN:
			{
				LoginResult inPacket ;
				mRecvBuffer.Read((char*)&inPacket, header.mSize) ;
				//////////////////////////////////////////////////////////////////////////
				// CircularBuffer에서 Peek는 읽기만 하는 것
				// Read는 읽고 나서, 읽은 만큼 제거
				//////////////////////////////////////////////////////////////////////////

				/// 로그인은 DB 작업을 거쳐야 하기 때문에 DB 작업 요청한다.
				LoadPlayerDataContext* newDbJob = new LoadPlayerDataContext(mSocket, inPacket.mPlayerId) ;
				GDatabaseJobManager->PushDatabaseJobRequest(newDbJob) ;
			
			}
			break ;

		case PKT_CS_CHAT:
			{
				ChatBroadcastRequest inPacket ;
				mRecvBuffer.Read((char*)&inPacket, header.mSize) ;
				
				ChatBroadcastResult outPacket ;
				outPacket.mPlayerId = inPacket.mPlayerId ;
				strcpy_s(outPacket.mName, mPlayerName) ;
				strcpy_s(outPacket.mChat, inPacket.mChat) ;
		
				/// 채팅은 바로 방송 하면 끝
				if ( !Broadcast(&outPacket) )
					return ;
 
			}
			break ;

		default:
			{
				/// 여기 들어오면 이상한 패킷 보낸거다.
				Disconnect() ;
				return ;
			}
			break ;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// ClientManager에서 BroadcastPacket() 하면 Send() 호출
//
// PostRecv() : 수신 / Send() : 송신
//////////////////////////////////////////////////////////////////////////
bool ClientSession::Send(PacketHeader* pkt)
{
	if ( !IsConnected() )
		return false ;

	/// 버퍼 용량 부족인 경우는 끊어버림
	if ( false == mSendBuffer.Write((char*)pkt, pkt->mSize) )
	{
		Disconnect() ;
		return false ;
	}

	/// 보낼 데이터가 있는지 검사
	if ( mSendBuffer.GetContiguiousBytes() == 0 )
	{
		/// 방금전에 write 했는데, 데이터가 없다면 뭔가 잘못된 것
		assert(false) ;
		Disconnect() ;
		return false ;
	}
		
	DWORD sendbytes = 0 ;
	DWORD flags = 0 ;

	WSABUF buf ;

	//////////////////////////////////////////////////////////////////////////
	// CircularBuffer.cpp 참조
	buf.len = (ULONG)mSendBuffer.GetContiguiousBytes() ;
	buf.buf = (char*)mSendBuffer.GetBufferStart() ;
	
	memset(&mOverlappedSend, 0, sizeof(OverlappedIO)) ;

	mOverlappedSend.mObject = this ;
	// PostRecv()에서와 동일

	//////////////////////////////////////////////////////////////////////////
	// 비동기 입출력 시작
	// 상단의 WSARecv() 참고
	//////////////////////////////////////////////////////////////////////////
	if ( SOCKET_ERROR == WSASend(mSocket, &buf, 1, &sendbytes, flags, &mOverlappedSend, SendCompletion) )
	{
		if ( WSAGetLastError() != WSA_IO_PENDING )
			return false ;
	}

	IncOverlappedRequest() ;
	// Overlapped IO 요청 했음. 카운트 증가

	return true ;
}

void ClientSession::OnWriteComplete(size_t len)
{
	/// 보내기 완료한 데이터는 버퍼에서 제거
	mSendBuffer.Remove(len) ;

	/// 얼래? 덜 보낸 경우도 있나? (커널의 send queue가 꽉찼거나, Send Completion이전에 또 send 한 경우?)
	if ( mSendBuffer.GetContiguiousBytes() > 0 )
	{
		assert(false) ;
		// 이러면 디버깅 모드에서는 무조건 이 안에 들어왔을 때 assert 되고 중지!
	}

}

bool ClientSession::Broadcast(PacketHeader* pkt)
{
	if ( !Send(pkt) )
		return false ;

	if ( !IsConnected() )
		return false ;

	GClientManager->BroadcastPacket(this, pkt) ;

	return true ;
}

void ClientSession::OnTick()
{
	/// 클라별로 주기적으로 해야 될 일은 여기에
	
	/// 특정 주기로 DB에 위치 저장
	if ( ++mDbUpdateCount == PLAYER_DB_UPDATE_CYCLE )
	// 현재 1초 마다 OnTick() 호출
	// 1분마다 플레이어 위치 저장
	{
		mDbUpdateCount = 0 ;
		UpdatePlayerDataContext* updatePlayer = new UpdatePlayerDataContext(mSocket, mPlayerId) ;

		updatePlayer->mPosX = mPosX ;
		updatePlayer->mPosY = mPosY ;
		updatePlayer->mPosZ = mPosZ ;
		strcpy_s(updatePlayer->mComment, "updated_test") ; ///< 일단은 테스트를 위해
		GDatabaseJobManager->PushDatabaseJobRequest(updatePlayer) ;
	}
	
}

void ClientSession::DatabaseJobDone(DatabaseJobContext* result)
{
	CRASH_ASSERT( mSocket == result->mSockKey ) ;
	

	const type_info& typeInfo = typeid(*result) ;

	if ( typeInfo == typeid(LoadPlayerDataContext) )
	{
		LoadPlayerDataContext* login = dynamic_cast<LoadPlayerDataContext*>(result) ;

		LoginDone(login->mPlayerId, login->mPosX, login->mPosY, login->mPosZ, login->mPlayerName) ;
	
	}
	else if ( typeInfo == typeid(UpdatePlayerDataContext) )
	{
		UpdateDone() ;
	}
	else
	{
		CRASH_ASSERT(false) ;
	}

}

void ClientSession::UpdateDone()
{
	/// 콘텐츠를 넣기 전까지는 딱히 해줄 것이 없다. 단지 테스트를 위해서..
	printf("DEBUG: Player[%d] Update Done\n", mPlayerId) ;
}


//////////////////////////////////////////////////////////////////////////
// 로그인을 시도해서 유저 데이터를 불러오면, 데이터를 읽어서 클라이언트 쪽에 Send()
//////////////////////////////////////////////////////////////////////////
void ClientSession::LoginDone(int pid, double x, double y, double z, const char* name)
{
	LoginResult outPacket ;

	outPacket.mPlayerId = mPlayerId = pid ;
	outPacket.mPosX = mPosX = x ;
	outPacket.mPosY = mPosY = y ;
	outPacket.mPosZ = mPosZ = z ;
	strcpy_s(mPlayerName, name) ;
	strcpy_s(outPacket.mName, name) ;

	Send(&outPacket) ;

	mLogon = true ;
}



//////////////////////////////////////////////////////////////////////////
// 비동기 입력 WSARecv()에 의해서, 입력이 완료 되면 콜백으로 RecvCompletion 실행
//////////////////////////////////////////////////////////////////////////
void CALLBACK RecvCompletion(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	ClientSession* fromClient = static_cast<OverlappedIO*>(lpOverlapped)->mObject ;
	//////////////////////////////////////////////////////////////////////////
	// lpOverlapped 인자를 OverlappedIO 형태로 형변환 하면
	// 해당 구조체의 멤버 변수 mObject => ClientSession*
	// 
	// 바로 이 포인터가 비동기 입력 WSARecv로 보낸 ClientSession 객체의 주소값
	// 
	// PostRecv 멤소드에서 mOverlappedRecv.mObject = this ; 이 부분 참조
	//////////////////////////////////////////////////////////////////////////
	fromClient->DecOverlappedRequest() ;
	// Overlapped IO 완료 했음. 카운트 감소

	if ( !fromClient->IsConnected() )
		return ;

	/// 에러 발생시 해당 세션 종료
	if ( dwError || cbTransferred == 0 )
	{
		fromClient->Disconnect() ;
		return ;
	}

	/// 받은 데이터 처리
	fromClient->OnRead(cbTransferred) ;

	/// 다시 받기
	if ( false == fromClient->PostRecv() )
	{
		fromClient->Disconnect() ;
		return ;
	}
}

//////////////////////////////////////////////////////////////////////////
// 비동기 출력 WSASend()에 의해서 출력이 완료 되면 콜백으로 SendCompletion 실행
//////////////////////////////////////////////////////////////////////////
void CALLBACK SendCompletion(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	ClientSession* fromClient = static_cast<OverlappedIO*>(lpOverlapped)->mObject ;

	//////////////////////////////////////////////////////////////////////////
	fromClient->DecOverlappedRequest() ;
	// Overlapped IO 완료 했음. 카운트 감소

	if ( !fromClient->IsConnected() )
		return ;

	/// 에러 발생시 해당 세션 종료
	if ( dwError || cbTransferred == 0 )
	{
		fromClient->Disconnect() ;
		return ;
	}

	fromClient->OnWriteComplete(cbTransferred) ;
	// 보내기 완료한 데이터를 버퍼에서 제거

}

