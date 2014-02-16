#include "stdafx.h"
#include "EasyServer.h"
#include "DatabaseJobContext.h"
#include "DatabaseJobManager.h"
#include "DbHelper.h"

DatabaseJobManager* GDatabaseJobManager = nullptr ;

//////////////////////////////////////////////////////////////////////////
// 아래 함수는 DB 처리 쓰레드에서 불려야 한다
//
// EasyServer.cpp 에 있는 DB 핸들링 스레드 참고
//////////////////////////////////////////////////////////////////////////
void DatabaseJobManager::ExecuteDatabaseJobs()
{
	assert( LThreadType == THREAD_DATABASE ) ;

	DatabaseJobContext* jobContext = nullptr ;
	while ( mDbJobRequestQueue.PopFront(jobContext) )
	{
		/// 여기서 DB호출해서 처리하고 
		jobContext->mSuccess = jobContext->OnExecute() ;
		// DatabaseJobContext.cpp 참고

		/// 그 결과를 result queue에 담아 놓음
		mDbJobResultQueue.PushBack(jobContext) ;
	}
}

//////////////////////////////////////////////////////////////////////////
// 하단의 두 함수가 불리는 것은?
//
// EasyServer.cpp 에 있는 클라이언트 핸들링 스레드에서 
//
// 1. SetWaitableTimer를 이용해서 0.1초마다 TimerProc 콜백
// 여기에서 GClientManager->OnPeriodWork() 실행
// 
// OnPeriodWork()에서 살펴보면
//
// ClientPeriodWork()에서 각 ClientSession에 OnTick() 호출
// ClientSession->OnTick()에서 DB 관련 처리
//
// DispatchDatabaseJobResults()에서 DB 관련 처리
//
// 2. 클라이언트가 접속해서 accept() 되고 나면
// OnConnect 쪽에서 PostRecv() 하게 되어서(ClientSession.cpp 참고) 통신 시작하고,
// 패킷 핸들링 하게 되면서 로그인 등에서 DB 관련 처리
//
//
// 위의 1, 2 두 가지 경우 모두 클라이언트 핸들링 스레드에서 호출 됨
//////////////////////////////////////////////////////////////////////////

/// 아래 함수는 클라이언트 처리 쓰레드에서 불려야 한다
bool DatabaseJobManager::PushDatabaseJobRequest(DatabaseJobContext* jobContext)
{
	assert( LThreadType == THREAD_CLIENT ) ;
	return mDbJobRequestQueue.PushBack(jobContext) ;
}

/// 아래 함수는 클라이언트 처리 쓰레드에서 불려야 한다
bool DatabaseJobManager::PopDatabaseJobResult(DatabaseJobContext*& jobContext)
{
	assert( LThreadType == THREAD_CLIENT ) ;
	return mDbJobResultQueue.PopFront(jobContext) ;
}