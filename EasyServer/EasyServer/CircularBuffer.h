#pragma once


class CircularBuffer
{
public:

	CircularBuffer(size_t capacity) : mBRegionPointer(nullptr), mARegionSize(0), mBRegionSize(0)
	{
		mBuffer = new char[capacity] ;
		// 문자열 배열을 BUFSIZE (1024 * 10) 크기 만큼 할당하고 시작 주소 mBuffer 에 저장

		mBufferEnd = mBuffer + capacity ;
		// 끝 주소값(= 시작 주소값 + 배열 사이즈) mBufferEnd 에 저장

		mARegionPointer = mBuffer ;
		// 일단 A영역 포인터를 버퍼의 시작 주소값에 넣는다.

		//////////////////////////////////////////////////////////////////////////
		// mBRegionPointer(nullptr), mARegionSize(0), mBRegionSize(0) 로 초기화
		//
		// B영역 포인터는 nullptr로 B영역은 아직 설정(사용) 되지 않음
		// A영역, B영역 모두 사용하지 않았으므로 사이즈는 0으로 초기화
		//////////////////////////////////////////////////////////////////////////

	}

	~CircularBuffer()
	{
		delete [] mBuffer ;
	}

	// 데이터 가져오기
	bool Peek(OUT char* destbuf, size_t bytes) const ;

	// 읽기(읽고 난 데이터 삭제)
	bool Read(OUT char* destbuf, size_t bytes) ;

	// 쓰기
	bool Write(const char* data, size_t bytes) ;

	/// 버퍼의 첫부분 len만큼 날리기
	void Remove(size_t len) ;

	size_t GetFreeSpaceSize()
	{
		if ( mBRegionPointer != nullptr )
			return GetBFreeSpace() ;
		// B영역에 데이터 들어있으면 B~A 사이에 가용 가능한 공간을 리턴
		else
		{
			/// A 버퍼보다 더 많이 존재하면, B 버퍼로 스위치
			//
			// 교수님 주석에 첨부하여 하단에 주석을 상세히 풀어 추가함
			
			//////////////////////////////////////////////////////////////////////////
			// A버퍼 시작 위치가 뒤로 많이 밀려났다면
			// (쓰고 읽고 앞부분부터 데이터 삭제를 반복하면 A 시작위치는 점점 뒤로 감)
			// B 영역을 써야 한다
			//
			// GetSpaceBeforeA <- [ A버퍼 ] -> GetAFreeSpace
			// 앞부분                                 뒷부분
			//
			// 즉, 위의 그림에서 앞부분 크기가 뒷부분 크기보다 커지면
			// 앞쪽 영역을 사용하기 위해서 앞쪽에 B영역을 지정해서 그곳을 사용한다.
			//
			// cpp 파일을 살펴보면, 구현 되어 있는 메소드들 내부에
			//
			// B버퍼가 비었으면 B버퍼를 제거하고 맨 앞부터 A영역으로 다시 설정하거나,
			// A버퍼가 비었으면 B버퍼를 맨 앞(시작위치)으로 당기고 A버퍼로 스위칭하는 코드가 있음
			//
			//
			// 즉 정리해보면
			//
			// 1. A버퍼를 계속 사용하면서 시작 위치가 뒤로 밀려나면
			// 2. 앞쪽에 B버퍼를 만들어서 사용함
			// 3. 어찌 되었든 버퍼를 비우고 나면 B를 제거하든 A가 B로 변하든 앞부분이 다시 A가 됨
			//
			// 위 과정을 반복하는 자료 구조
			//////////////////////////////////////////////////////////////////////////
			if ( GetAFreeSpace() < GetSpaceBeforeA() )
			{
				AllocateB() ;
				return GetBFreeSpace() ;
				// 앞쪽 공간이 크면 앞에 B버퍼 설정하고, B버퍼를 쓸 수 있는 크기를 리턴하자
			}
			else
				return GetAFreeSpace() ;
				// 아니면 무난하게 A버퍼를 쓸 수 있는 크기를 리턴하자
		}
	}

	size_t GetStoredSize() const
	{
		return mARegionSize + mBRegionSize ;
		// A영역과 B영역 사용한 크기 총합
	}

	size_t GetContiguiousBytes() const 
	{
		if ( mARegionSize > 0 )
			return mARegionSize ;
		// A영역에 데이터가 들어있으면 A영역 사이즈를
		else
			return mBRegionSize ;
		// A영역이 비어 있으면 B영역 사이즈를 리턴
	}

	/// 쓰기가 가능한 위치 (버퍼의 끝부분) 반환
	void* GetBuffer() const
	{
		if( mBRegionPointer != nullptr )
			return mBRegionPointer + mBRegionSize ;
		// 현재 B영역을 쓰고 있을 때
		else
			return mARegionPointer + mARegionSize ;
		// 현재 A영역을 쓰고 있을 때
	}

	/// 커밋(aka. IncrementWritten)
	// 버퍼 영역 사이즈 증가 (쓰기 발생)
	void Commit(size_t len)
	{
		if ( mBRegionPointer != nullptr )
			mBRegionSize += len ;
		// B영역을 쓰고 있을 때
		else
			mARegionSize += len ;
		// A영역을 쓰고 있을 때
	}

	/// 버퍼의 첫부분 리턴
	void* GetBufferStart() const
	{
		if ( mARegionSize > 0 )
			return mARegionPointer ;
		// A영역에 데이터가 들어있으면 일단 A영역 시작 주소를 리턴한다
		else
			return mBRegionPointer ;
		// 아니면 B영역 시작 주소를 리턴한다.
	}


private:

	//////////////////////////////////////////////////////////////////////////
	size_t GetAFreeSpace() const
	{ 
		return (mBufferEnd - mARegionPointer - mARegionSize) ; 
		//////////////////////////////////////////////////////////////////////////
		// mBufferEnd - mARegionPointer - mARegionSize
		// = mBufferEnd - (mARegionPointer + mARegionSize)
		// = mBufferEnd - mARegionEnd(이런 변수는 없음 = 개념 상으로... A영역 끝 주소)
		//
		// A영역 끝부터 (-> 방향으로) 버퍼 마지막 위치까지 남은 공간 크기
		//////////////////////////////////////////////////////////////////////////
	}

	size_t GetSpaceBeforeA() const
	{ 
		return (mARegionPointer - mBuffer) ; 
		// A영역 시작부터 (<- 방향으로) 버퍼 시작 위치까지 남은 공간 크기
	}
	// 위의 자료 구조를 그림으로 그려보면?
	//
	// 버퍼 시작위치               A영역 시작위치                  A영역 끝위치                   버퍼 끝위치
	// mBuffer -(GetSpaceBeforeA)- mARegionPointer -(mARegionSize)-[mARegionEnd]-(GetAFreeSpace)- mBufferEnd
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void AllocateB()
	{
		mBRegionPointer = mBuffer ;
		// B영역 시작 주소를 mBuffer(버퍼 시작 주소)로 설정
	}

	size_t GetBFreeSpace() const
	{
		if (mBRegionPointer == nullptr)
			return 0 ; 
		// B영역을 아직 사용하지 않았다면 0

		return (mARegionPointer - mBRegionPointer - mBRegionSize) ; 
		//////////////////////////////////////////////////////////////////////////
		// mARegionPointer - mBRegionPointer - mBRegionSize
		// = mARegionPointer - (mBRegionPointer + mBRegionSize)
		// = mARegionPointer - mBRegionEnd(물론 이런 변수가 없음. 개념상 B영역 끝 주소)
		//
		// B영역 시작부터 (-> 방향으로) A영역 시작 위치까지 남은 공간 크기
		//////////////////////////////////////////////////////////////////////////
	}

	// 위의 자료 구조를 그림으로 그려보면?
	//
	// [전략]- mBRegionPointer -(mBRegionSize)-[mBRegionEnd]-(GetBFreeSpace)- mARegionPointer -[후략]
	//////////////////////////////////////////////////////////////////////////

private:

	char*	mBuffer ;
	char*	mBufferEnd ;

	char*	mARegionPointer ;
	size_t	mARegionSize ;

	char*	mBRegionPointer ;
	size_t	mBRegionSize ;

} ;
