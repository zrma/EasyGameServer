#include "stdafx.h"
#include "CircularBuffer.h"
#include <assert.h>

//////////////////////////////////////////////////////////////////////////
// 정해진 크기만큼 읽어만 오기
// 
// 패킷 헤더를 읽어올 때 사용
//////////////////////////////////////////////////////////////////////////
bool CircularBuffer::Peek(OUT char* destbuf, size_t bytes) const
{
	assert( mBuffer != nullptr ) ;
	//////////////////////////////////////////////////////////////////////////
	// 생성자에서 new 했어야 됐다.
	// nullptr이면 동적 메모리 할당에 문제가 있는 것
	//////////////////////////////////////////////////////////////////////////

	if( mARegionSize + mBRegionSize < bytes )
		return false ;
	// bytes 만큼 읽기와 보기로 했는데 버퍼 총 영역 사이즈가 이것보다 작다.
	//
	// 아직 패킷 헤더 크기만큼 데이터가 오지도 않은 것임

	size_t cnt = bytes ;
	size_t aRead = 0 ;

	/// A, B 영역 둘다 데이터가 있는 경우는 A먼저 읽는다
	if ( mARegionSize > 0 )
	{
		aRead = (cnt > mARegionSize) ? mARegionSize : cnt ;
		//////////////////////////////////////////////////////////////////////////
		// 읽으려는 크기(cnt)가 A영역에 담긴 데이터 양보다 크면 - case 1
		// 일단 읽는 양(aRead)을 A영역 사이즈로 설정
		// 아니라면 읽으려는 크기만큼 전체 읽기
		//
		// case 1에서 나머지 데이터는 B 영역에 있을 것임
		//////////////////////////////////////////////////////////////////////////
		memcpy(destbuf, mARegionPointer, aRead) ;
		cnt -= aRead ;
		// cnt > 0이라면, B영역에서 나머지 데이터를 읽어 와야 됨
	}

	/// 읽기 요구한 데이터가 더 있다면 B 영역에서 읽는다
	if ( cnt > 0 && mBRegionSize > 0 )
	{
		assert(cnt <= mBRegionSize) ;
		// A 읽어오고 B를 읽은 건데
		// 읽을 데이터 양이 B에 담겨 있는 전체 데이터 양보다 크다면 이상한 거다.

		/// 남은거 마저 다 읽기
		size_t bRead = cnt ;

		memcpy(destbuf + aRead, mBRegionPointer, bRead) ;
		cnt -= bRead ;
	}

	assert( cnt == 0 ) ;
	// A버퍼 + B버퍼 데이터 싹 다 읽어와서 지정한 크기만큼 모두 읽었어야 하는데
	// cnt(읽을 양 남은 카운트 수) 가 0 이 아니면 잘못 된 거다.
	//
	// 저 위의 if( mARegionSize + mBRegionSize < bytes ) return false ; 구문에서 걸러졌어야 됨

	return true ;

}

//////////////////////////////////////////////////////////////////////////
// 위의 Peek가 헤더를 일단 읽어본 후에, 헤더에서 담고 있는 mSize 만큼 버퍼에 들어있다면
// 그 부분이 실질적인 패킷 데이터 부분이므로 아래의 Read() 메소드로 읽어옴
//////////////////////////////////////////////////////////////////////////
bool CircularBuffer::Read(OUT char* destbuf, size_t bytes)
{
	assert( mBuffer != nullptr ) ;

	if( mARegionSize + mBRegionSize < bytes )
		return false ;
	// 여기까지는 Peek() 와 동일한 방어코드

	size_t cnt = bytes ;
	size_t aRead = 0 ;

	/// A, B 영역 둘다 데이터가 있는 경우는 A먼저 읽는다
	if ( mARegionSize > 0 )
	{
		aRead = (cnt > mARegionSize) ? mARegionSize : cnt ;
		memcpy(destbuf, mARegionPointer, aRead) ;
		
		//////////////////////////////////////////////////////////////////////////
		mARegionSize -= aRead ;
		mARegionPointer += aRead ;
		//////////////////////////////////////////////////////////////////////////
		// Peek() 메소드와 다르게 더 추가 된 부분
		//
		// A버퍼 읽었으니 A버퍼 시작 포인터는 읽은 만큼(aRead) 증가시키고
		// A버퍼의 사이즈는 감소 시킨다.
		//
		// A버퍼 시작부분--(읽은 만큼 우측으로 이동)--[A버퍼 사이즈]--> A버퍼 끝부분 (고정 됨)
		//////////////////////////////////////////////////////////////////////////

		cnt -= aRead ;
	}
	
	/// 읽기 요구한 데이터가 더 있다면 B 영역에서 읽는다
	if ( cnt > 0 && mBRegionSize > 0 )
	{
		assert(cnt <= mBRegionSize) ;

		/// 남은거 마저 다 읽기
		size_t bRead = cnt ;

		memcpy(destbuf+aRead, mBRegionPointer, bRead) ;

		//////////////////////////////////////////////////////////////////////////
		mBRegionSize -= bRead ;
		mBRegionPointer += bRead ;
		//////////////////////////////////////////////////////////////////////////
		// 마찬가지로 Peek() 메소드와 다르게 더 추가 된 부분
		//
		// 버퍼 안의 데이터들을 읽은 후에 제거시킴(실제로는 가리키는 포인터 이동)
		//////////////////////////////////////////////////////////////////////////

		cnt -= bRead ;
	}

	assert( cnt == 0 ) ;

	/// A 버퍼가 비었다면 B버퍼를 맨 앞으로 당기고 A 버퍼로 지정 
	if ( mARegionSize == 0 )
	{
		// B버퍼에 데이터가 있을 때
		if ( mBRegionSize > 0 )
		{
			// B버퍼가 맨 앞이 아니면
			if ( mBRegionPointer != mBuffer )
				memmove(mBuffer, mBRegionPointer, mBRegionSize) ;
			//////////////////////////////////////////////////////////////////////////
			// http://kks227.blog.me/60207310880
			// http://www.borlandforum.com/impboard/impboard.dll?action=read&db=bcb_tip&no=826
			// http://young_0620.blog.me/50174961382 참조
			//////////////////////////////////////////////////////////////////////////

			mARegionPointer = mBuffer ;
			mARegionSize = mBRegionSize ;
			// B버퍼를 A버퍼로 바꿈

			mBRegionPointer = nullptr ;
			mBRegionSize = 0 ;
			// B버퍼는 초기화

			// 이 상태가 되면 A버퍼에만 데이터가 남아 있는 상태임
		}
		else
		{
			/// B에 아무것도 없는 경우 그냥 A로 스위치
			mBRegionPointer = nullptr ;
			mBRegionSize = 0 ;
			// B버퍼에 아무 것도 없으므로 바로 초기화

			mARegionPointer = mBuffer ;
			mARegionSize = 0 ;
			// A버퍼가 이미 비어 있으므로 A버퍼도 초기화

			// 이 상태가 되면 CircularBuffer() 초기 생성 상태임
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 결과적으로 위의 if 구문을 수행하고 나면
	// B버퍼는 사라지고, A버퍼만 남아 있는(데이터가 있는지는 차지하고서라도)
	// 상태로 넘어오게 된다.
	//
	// 순서!
	//
	// 언제나 데이터는 A버퍼부터 읽어들이고,
	// 읽어들이려고 했던 데이터 사이즈가 A버퍼보다 크다면 B에서 마저 읽는다.
	//
	// 만약 A버퍼 + B버퍼 다 합친 크기보다 더 많이 읽으려고 하면,
	// 위에서 방어코드 에서 걸러졌어야 함
	//////////////////////////////////////////////////////////////////////////

	return true ;
}




bool CircularBuffer::Write(const char* data, size_t bytes)
{
	assert( mBuffer != nullptr ) ;

	/// Read와 반대로 B가 있다면 B영역에 먼저 쓴다
	if( mBRegionPointer != nullptr )
	{
		if ( GetBFreeSpace() < bytes )
			return false ;

		memcpy(mBRegionPointer + mBRegionSize, data, bytes) ;
		mBRegionSize += bytes ;

		return true ;
	}

	/// A영역보다 다른 영역의 용량이 더 클 경우 그 영역을 B로 설정하고 기록
	if ( GetAFreeSpace() < GetSpaceBeforeA() )
	{
		AllocateB() ;

		if ( GetBFreeSpace() < bytes )
			return false ;

		memcpy(mBRegionPointer + mBRegionSize, data, bytes) ;
		mBRegionSize += bytes ;

		return true ;
	}
	/// A영역이 더 크면 당연히 A에 쓰기
	else
	{
		if ( GetAFreeSpace() < bytes )
			return false ;

		memcpy(mARegionPointer + mARegionSize, data, bytes) ;
		mARegionSize += bytes ;

		return true ;
	}
}



void CircularBuffer::Remove(size_t len)
{
	size_t cnt = len ;
	
	/// Read와 마찬가지로 A가 있다면 A영역에서 먼저 삭제

	if ( mARegionSize > 0 )
	{
		size_t aRemove = (cnt > mARegionSize) ? mARegionSize : cnt ;
		mARegionSize -= aRemove ;
		mARegionPointer += aRemove ;
		cnt -= aRemove ;
	}

	// 제거할 용량이 더 남은경우 B에서 제거 
	if ( cnt > 0 && mBRegionSize > 0 )
	{
		size_t bRemove = (cnt > mBRegionSize) ? mBRegionSize : cnt ;
		mBRegionSize -= bRemove ;
		mBRegionPointer += bRemove ;
		cnt -= bRemove ;
	}

	/// A영역이 비워지면 B를 A로 스위치 
	if ( mARegionSize == 0 )
	{
		if ( mBRegionSize > 0 )
		{
			/// 앞으로 당겨 붙이기
			if ( mBRegionPointer != mBuffer )
				memmove(mBuffer, mBRegionPointer, mBRegionSize) ;
	
			mARegionPointer = mBuffer ;
			mARegionSize = mBRegionSize ;
			mBRegionPointer = nullptr ;
			mBRegionSize = 0 ;
		}
		else
		{
			mBRegionPointer = nullptr ;
			mBRegionSize = 0 ;
			mARegionPointer = mBuffer ;
			mARegionSize = 0 ;
		}
	}
}


