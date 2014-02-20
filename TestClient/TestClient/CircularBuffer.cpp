#include "stdafx.h"
#include "CircularBuffer.h"


CircularBuffer::CircularBuffer(size_t capacity)
  : mBeginIndex(0), mEndIndex(0), mCurrentSize(0), mCapacity(capacity)
{
	mData = new char[capacity] ;
}

CircularBuffer::~CircularBuffer()
{
	delete [] mData ;
}

bool CircularBuffer::Write(const char* data, size_t bytes)
{
	if (bytes == 0)
		return false ;

	/// 용량 부족
	if ( bytes > mCapacity - mCurrentSize )
		return false ;
	
	// 바로 쓰기 가능한 경우
	//////////////////////////////////////////////////////////////////////////
	// 끝부분 = mCapacity
	// 현재 데이터가 차 있는 끝 부분 = mEndIndex
	//
	// 현재 데이터 꽉 차 있는 끝부분부터 버퍼 끝까지 남은 공간 사이 - 1
	// = mCapacity - mEndIndex
	//////////////////////////////////////////////////////////////////////////
	if ( bytes <= mCapacity - mEndIndex )
	{
		memcpy(mData + mEndIndex, data, bytes) ;
		mEndIndex += bytes ;

		if ( mEndIndex == mCapacity )
			mEndIndex = 0 ;
		// 데이터가 꽉 차면 mEndIndex를 0으로 보내버림
		// 서큘러 큐 형태
	}
	// 쪼개서 써야 될 경우
	else
	{
		// 뒷부분 남은 공간 - 위의 주석 - 1 참조
		size_t size1 = mCapacity - mEndIndex ;
		memcpy(mData + mEndIndex, data, size1) ;
		
		// 뒷부분 꽉 채웠으니 나머지 데이터는 시작 부분부터 앞부터 채워나감
		size_t size2 = bytes - size1 ;
		memcpy(mData, data + size1, size2) ;
		mEndIndex = size2 ;
	}

	mCurrentSize += bytes ;

	return true ;
}

bool CircularBuffer::Read(char* data, size_t bytes)
{
	if (bytes == 0)
		return false ;

	if ( mCurrentSize < bytes )
		return false ;

	/// 바로 한번에 읽어 올 수 있는 경우
	//////////////////////////////////////////////////////////////////////////
	// 버퍼의 끝부분 = mCapacity 
	// 현재 데이터가 차 있는 시작부분 = mBeginIndex
	//
	// 위에서 mCurrentSize < bytes 이니까
	// bytes <= mCapacity - mBeginIndex 라면 데이터 차 있는 시작부분부터 버퍼 끝까지
	// 꽉 차 있는 데이터 안에서 한 번에 bytes를 긁어 올 수 있겠지
	//////////////////////////////////////////////////////////////////////////
	if ( bytes <= mCapacity - mBeginIndex )
	{
		memcpy(data, mData + mBeginIndex, bytes) ;
		mBeginIndex += bytes ;

		if ( mBeginIndex == mCapacity )
			mBeginIndex = 0 ;
		// 서큘러 큐 형태
	}
	/// 읽어올 데이터가 쪼개져 있는 경우
	else
	{
		// 이건 시작부분부터 버퍼 메모리의 맨 끝까지
		size_t size1 = mCapacity - mBeginIndex ;
		memcpy(data, mData + mBeginIndex, size1) ;

		// 이건 버퍼 메모리 시작위치부터 나머지 읽을 사이즈까지
		size_t size2 = bytes - size1 ;
		memcpy(data + size1, mData, size2) ;
		mBeginIndex = size2 ;
	}

	mCurrentSize -= bytes ;

	return true ;
}

//////////////////////////////////////////////////////////////////////////
// 데이터를 읽기만 하므로 mBeginIndex, mEndIndex, mCurrentSize 등
// 내부 변수 변경이 없다.
//////////////////////////////////////////////////////////////////////////

// 데이터 들어 있는 만큼 통째로 Peek
void CircularBuffer::Peek(char* data)
{
	/// 바로 한번에 읽어 올 수 있는 경우
	if ( mCurrentSize <= mCapacity - mBeginIndex )
	{
		memcpy(data, mData + mBeginIndex, mCurrentSize) ;
	}
	/// 읽어올 데이터가 쪼개져 있는 경우
	else
	{
		size_t size1 = mCapacity - mBeginIndex ;
		memcpy(data, mData + mBeginIndex, size1) ;

		size_t size2 = mCurrentSize - size1 ;
		memcpy(data + size1, mData, size2) ;
	}
}

// 지정한 크기 만큼 Peek()
bool CircularBuffer::Peek(char* data, size_t bytes)
{
	if (bytes == 0)
		return false ;

	if ( mCurrentSize < bytes )
		return false ;

	/// 바로 한번에 읽어 올 수 있는 경우
	if ( bytes <= mCapacity - mBeginIndex )
	{
		memcpy(data, mData + mBeginIndex, bytes) ;
	}
	/// 읽어올 데이터가 쪼개져 있는 경우
	else
	{
		size_t size1 = mCapacity - mBeginIndex ;
		memcpy(data, mData + mBeginIndex, size1) ;

		size_t size2 = bytes - size1 ;
		memcpy(data + size1, mData, size2) ;
	}

	return true ;
}


bool CircularBuffer::Consume(size_t bytes)
{
	if (bytes == 0)
		return false ;

	if ( mCurrentSize < bytes )
		return false ;

	/// 바로 한번에 제거할 수 있는 경우
	if ( bytes <= mCapacity - mBeginIndex )
	{
		mBeginIndex += bytes ;

		if ( mBeginIndex == mCapacity )
			mBeginIndex = 0 ;
	}
	/// 제거할 데이터가 쪼개져 있는 경우
	else
	{
		size_t size2 = bytes + mBeginIndex - mCapacity ;
		//////////////////////////////////////////////////////////////////////////
		// 제거할 데이터가 쪼개져 있다는 이야기는
		// mBeginIndex + bytes 하면 서큘러 연산처럼 넘어간단 뜻이므로
		// 모듈러 효과를 주기 위해 -mCapacity 해줘야 됨
		//////////////////////////////////////////////////////////////////////////
		mBeginIndex = size2 ;
	}

	mCurrentSize -= bytes ;

	return true ;

}