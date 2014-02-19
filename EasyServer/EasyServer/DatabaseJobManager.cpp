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
//
// 멤버변수로 갖고 있는, DB 작업 요청 큐(mDbJobRequestQueue) 안의 
// 작업 대기 되어 있는 DB Job Context 들을 하나씩 꺼내서 처리 한 후 
// 작업 완료 큐 (mDbJobResultQueue) 로 집어넣는다.
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
//
// 위의 두 가지 경우 모두, 즉 아래 하단의 2개 함수 모두
// DB 작업 매니저의 멤버 변수인 큐에 작업을 넣거나 꺼내는 역할만 함
//
// 큐 자료구조의 PushBack() 과 PopFront() 를 함수로 감싸놓은 상태
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