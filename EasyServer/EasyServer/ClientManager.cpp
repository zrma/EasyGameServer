﻿#include "stdafx.h"
#include "EasyServer.h"
#include "..\..\PacketType.h"
#include "ClientSession.h"
#include "ClientManager.h"
#include "DatabaseJobContext.h"
#include "DatabaseJobManager.h"

//////////////////////////////////////////////////////////////////////////
// 클라이언트 매니저
//////////////////////////////////////////////////////////////////////////

ClientManager* GClientManager = nullptr ;
//////////////////////////////////////////////////////////////////////////
// 헤더파일에 extern 형태로 선언 되어 있음
// 다른 부분에서도 가져다 쓰기 때문에 헤더에 extern 선언만 해 두고 여기서 nullptr로 초기화
//
// 실제 할당은 EasyServer.cpp의 _tmain() 함수에서 동적할당(new)


//////////////////////////////////////////////////////////////////////////
// 클라 생성
//
// _tmain() 쪽의 클라이언트 핸들링 스레드에서 WaitForSingleObjectEx(hEvent, INFINITE, TRUE) 가
// 이벤트를 발생하기를 기다려서 이벤트 발생 신호가 오면(클라이언트 쪽에서 접속 요청하면)
//
// 전역 변수 g_AcceptedSocket에 담겨 있던 소켓 데이터를 인자로 넘김
//////////////////////////////////////////////////////////////////////////
ClientSession* ClientManager::CreateClient(SOCKET sock)
{
	// 이 함수는 클라이언트 핸들링 스레드에서 불려야 한다.
	assert(LThreadType == THREAD_CLIENT);

	ClientSession* client = new ClientSession(sock) ;
	// 클라이언트 세션을 생성

	mClientList.insert(ClientList::value_type(sock, client)) ;
	//////////////////////////////////////////////////////////////////////////
	// 클라이언트 리스트에 소켓 데이터와 세션을 저장
	// 키는 소켓, 밸류는 클라이언트 세션의 포인터인 맵 자료구조
	//
	// typedef pair<const Key, Type> value_type;
	// value_type은 키와 밸류를 한 쌍으로 묶어주는 자료구조
	//////////////////////////////////////////////////////////////////////////

	return client ;
}

void ClientManager::BroadcastPacket(ClientSession* from, PacketHeader* pkt)
{
	/// FYI : C++ STL iterator 스타일의 루프
	// 연산자 오버로딩 된 ++ 연산자를 이용해서 차례대로 순회
	for ( ClientList::const_iterator it = mClientList.begin() ; it != mClientList.end() ; ++it )
	{
		ClientSession* client = it->second ;
		// ClientList는 맵이므로 it(iterator)는 해당 pair의 포인터이다.
		// 그러므로 it->second 는 밸류(클라이언트 세션)
		
		if ( from == client )
			continue ;
		// 보낸 이에게는 패스
		
		client->SendRequest(pkt);
		// ClientSession.cpp 참고
	}
}


//////////////////////////////////////////////////////////////////////////
// EasyServer.cpp의 클라이언트 핸들링 스레드에서 0.01초마다 콜백
//
// 0.01초마다 주기적으로 해야 할 일
//
// 1. 가비지 컬렉팅
// 2. 클라이언트 세션 별 할 일 처리
// 3. DB 작업 처리 된 것 각각 클라이언트에 맞게 적용
//////////////////////////////////////////////////////////////////////////
void ClientManager::OnPeriodWork()
{
	/// 접속이 끊긴 세션들 주기적으로 정리 (1초 정도 마다 해주자)
	DWORD currTick = GetTickCount() ;
	if ( currTick - mLastGCTick >= 1000 )
	{
		CollectGarbageSessions() ;
		mLastGCTick = currTick ;
	}

	/// 접속된 클라이언트 세션별로 주기적으로 해줘야 하는 일 (주기는 알아서 정하면 됨 - 지금은 1초로 ㅎㅎ)
	if ( currTick - mLastClientWorkTick >= 1000 )
	{
		ClientPeriodWork() ;
		mLastClientWorkTick = currTick ;
	}

	/// 처리 완료된 DB 작업들 각각의 Client로 dispatch
	DispatchDatabaseJobResults() ;
}

void ClientManager::CollectGarbageSessions()
{
	std::vector<ClientSession*> disconnectedSessions ;
	
	/// FYI : C++ 11 람다를 이용한 스타일
	//////////////////////////////////////////////////////////////////////////
	// for_each 사용법
	// http://psychoria.blog.me/40167726390 참조
	//////////////////////////////////////////////////////////////////////////
	// 람다 사용법
	// http://psychoria.blog.me/40171894460
	// http://psychoria.blog.me/40172034934 참조
	//////////////////////////////////////////////////////////////////////////
	std::for_each ( mClientList.begin() , mClientList.end() , 
		[&](ClientList::const_reference it )
		// Call by Reference(&)를 이용해서 이터레이터 람다 사용, 아래 함수 실행
		{
			ClientSession* client = it.second ;

			if ( false == client->IsConnected() && false == client->DoingOverlappedOperation() )
				disconnectedSessions.push_back(client) ;
		}
	) ;
	

	/// FYI : C언어 스타일의 루프
	for ( size_t i = 0 ; i < disconnectedSessions.size() ; ++i )
	{
		ClientSession* client = disconnectedSessions[i] ;
		// vector 자료구조에 대해서 연산자 오버로딩 된 [] 연산자를 통해 배열처럼 사용

		mClientList.erase(client->mSocket) ;
		// 우선 클라이언트 리스트에서 해당 목록을 지운다

		delete client ;
		//////////////////////////////////////////////////////////////////////////
		// 삭제 예정 벡터는 스택에 들어가 있다.
		// 하지만 동적 메모리 영역에 저장 된 내용물은 지워지지 않았으므로 여기서 delete
		//////////////////////////////////////////////////////////////////////////
	}
}

// 클라이언트 세션 별로 주기적으로 할 일
void ClientManager::ClientPeriodWork()
{
	/// FYI : C++ 11 스타일의 루프
	for (auto& it : mClientList)
	{
		ClientSession* client = it.second ;
		client->OnTick() ;
	}
}

void ClientManager::DispatchDatabaseJobResults()
{
	/// 쌓여 있는 DB 작업 처리 결과들을 각각의 클라에게 넘긴다
	DatabaseJobContext* dbResult = nullptr ;
	// 추상 클래스 포인터 선언 및 초기화

	// DB Job 매니저에서 계속 Pop
	while ( GDatabaseJobManager->PopDatabaseJobResult(dbResult) )
	{
		if ( false == dbResult->mSuccess )
		{
			printf("DB JOB FAIL \n") ;
		}
		else
		{
			//////////////////////////////////////////////////////////////////////////
			// typeid 를 사용하기 위해 typeinfo.h 인클루드
			// stdafx.h 에 #include <typeinfo> 되어 있음
			//
			// class의 타입을 알기 위한 키워드
			//
			// DatabaseJobContext가 추상 클래스이므로 해당 클래스를 상속 받은 하위 클래스의
			// 타입에 따라서 수행해야 할 함수들을 다르게 지정 할 수 있음
			//////////////////////////////////////////////////////////////////////////

			if ( typeid(*dbResult) == typeid(CreatePlayerDataContext) )
			{
				CreatePlayerDone(dbResult) ;
				// ClientManager 객체에서 처리
			}
			else if ( typeid(*dbResult) == typeid(DeletePlayerDataContext) )
			{
				DeletePlayerDone(dbResult) ;
				// ClientManager 객체에서 처리
			}
			else
			{
				/// 여기는 해당 DB요청을 했던 클라이언트에서 직접 해줘야 하는 경우다
				auto& it = mClientList.find(dbResult->mSockKey) ;

				if ( it != mClientList.end() && it->second->IsConnected() )
				{
					/// dispatch here....
					it->second->DatabaseJobDone(dbResult) ;
					//////////////////////////////////////////////////////////////////////////
					// ClientSession.cpp 파일 참조
					//
					// LoadPlayerDataContext
					// UpdatePlayerDataContext
					//
					// 위의 두 케이스는 여기(각각 ClientSession 객체)에서 처리
					//////////////////////////////////////////////////////////////////////////
				}
			}
		}
	
		/// 완료된 DB 작업 컨텍스트는 삭제해주자
		DatabaseJobContext* toBeDelete = dbResult ;
		delete toBeDelete ;
	}
}

void ClientManager::FlushClientSend()
{
	for (auto& it : mClientList)
	{
		ClientSession* client = it.second;
		if (false == client->SendFlush())
		{
			client->Disconnect();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// DB쪽에 요청 처리하는 이해를 돕기 위한 주석
// 
// Push Request / Pop Result 로 구성 되어 있음
// DatabaseJobManager.cpp 참조
//////////////////////////////////////////////////////////////////////////

void ClientManager::CreatePlayer(int pid, double x, double y, double z, const char* name, const char* comment)
{
	CreatePlayerDataContext* newPlayerJob = new CreatePlayerDataContext() ;
	newPlayerJob->mPlayerId = pid ;
	newPlayerJob->mPosX = x ;
	newPlayerJob->mPosY = y ;
	newPlayerJob->mPosZ = z ;
	strcpy_s(newPlayerJob->mPlayerName, name) ;
	strcpy_s(newPlayerJob->mComment, comment) ;

	GDatabaseJobManager->PushDatabaseJobRequest(newPlayerJob) ;
}

//////////////////////////////////////////////////////////////////////////
// 삭제 컨텍스트를 만들어서 해당 객체를 Push Request 함수를 이용해서 처리 요청
//////////////////////////////////////////////////////////////////////////
void ClientManager::DeletePlayer(int pid)
{
	DeletePlayerDataContext* delPlayerJob = new DeletePlayerDataContext(pid) ;
	GDatabaseJobManager->PushDatabaseJobRequest(delPlayerJob) ;
}

void ClientManager::CreatePlayerDone(DatabaseJobContext* dbJob)
{
	CreatePlayerDataContext* createJob = dynamic_cast<CreatePlayerDataContext*>(dbJob) ;
	// 부모의 추상화 클래스의 포인터를 인자로 받았으므로 dynamic_cast로 클래스 형 변환 해서
	// 자식 클래스인 CreatePlayerDataContext로 캐스팅

	printf("PLAYER[%d] CREATED: %s \n", createJob->mPlayerId, createJob->mPlayerName) ;
}

void ClientManager::DeletePlayerDone(DatabaseJobContext* dbJob)
{
	DeletePlayerDataContext* deleteJob = dynamic_cast<DeletePlayerDataContext*>(dbJob) ;
	// 위의 경우와 같음
	
	printf("PLAYER [%d] DELETED\n", deleteJob->mPlayerId) ;

}