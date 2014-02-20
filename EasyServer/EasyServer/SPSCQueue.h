#pragma once

#include <atomic>

//////////////////////////////////////////////////////////////////////////
// 이것은 atomic 관련해서 전반적으로 교수님께 설명 들어야 할 듯
//////////////////////////////////////////////////////////////////////////

template<typename TElem, int QSize>
class SPSCQueue
{
public:
	SPSCQueue() :mHeadPos(0), mTailPos(0) {}
	~SPSCQueue() {}

	/// 성공시 true, 꽉찼을 경우 false
	bool PushBack(const TElem& item) ;

	/// 성공시 true, 비어있을 경우 false
	bool PopFront(TElem& item) ;

private:

	std::atomic<int>	mHeadPos ; ///< for pop_front
	std::atomic<int>	mTailPos ; ///< for push_back
	//////////////////////////////////////////////////////////////////////////
	// http://msdn.microsoft.com/ko-kr/library/hh874894.aspx 참고
	//////////////////////////////////////////////////////////////////////////

	TElem mQueueArray[QSize+1] ;
} ;

template<typename TElem, int QSize>
bool SPSCQueue<TElem, QSize>::PushBack(const TElem& item)
{
	/// 큐의 뒤에다 삽입
	int currTailPos = mTailPos.load(std::memory_order_relaxed) ; 
	// memory_order_relaxed : 메모리 배치에 관여하지 않음

	//////////////////////////////////////////////////////////////////////////
	// http://msdn.microsoft.com/ko-kr/library/hh874894.aspx
	// http://yh120.tistory.com/10 참조
	//
	// atomic.load() : 원자적으로 값을 검색

	/// 큐의 마지막 원소는 full/empty여부를 가리기 위한 빈 공간으로 해놓기 때문에 QSize+1한다
	int nextTailPos = (currTailPos + 1) % (QSize + 1) ; 

	if ( nextTailPos == mHeadPos.load(std::memory_order_acquire) )
	// memory_order_acquire : 메모리 오더링
	{
		/// tail+1 == head인 경우이므로 큐 꽉찼다
		return false ;
	}

	mQueueArray[currTailPos] = item ;
	mTailPos.store(nextTailPos, std::memory_order_release) ;
	// 원자적으로 값을 저장
	
	return true ;
}

template<typename TElem, int QSize>
bool SPSCQueue<TElem, QSize>::PopFront(TElem& item)
{
	int currHeadPos = mHeadPos.load(std::memory_order_relaxed) ;

	if ( currHeadPos == mTailPos.load(std::memory_order_acquire) ) 
	{
		/// head == tail인 경우이므로 큐에 내용이 없다 
		return false ;
	}
	
	item = mQueueArray[currHeadPos] ;

	///  push에서와 같은 이유로..  QSize+1 해주는거임
	int nextHeadPos = (currHeadPos + 1) % (QSize + 1) ;

	mHeadPos.store(nextHeadPos, std::memory_order_release) ; 

	return true ;
}
