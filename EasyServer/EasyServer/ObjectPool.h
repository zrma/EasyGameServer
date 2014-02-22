#pragma once

#include "Exception.h"

// http://blog.secmem.org/104 참조
template <class TOBJECT, int ALLOC_COUNT=10>
class ObjectPool
{
public:

	// C++ 연산자 오버로딩
	// new 연산자 오버로딩
	static void* operator new(size_t objSize)
	{
		// 초기 생성
		if (!mFreeList)
		{
			mFreeList = new uint8_t[sizeof(TOBJECT)*ALLOC_COUNT] ;
			// TOBJECT의 10칸 배열을 미리 동적 생성 해 둠

			uint8_t* pNext = mFreeList ;
			// 엄밀히 따지면 mFreeList는 uint8_t(unsigned char) 배열이므로
			// 2중 배열 형태로 만들 필요가 있음

			//////////////////////////////////////////////////////////////////////////
			// 개요도를 그려 보면
			//
			// 할당 된 메모리는 사실
			//
			// [                                            ]  <- 내용물은 unsigned char
			//
			// 이것을
			//
			// [ OBJECT | OBJECT | (중략) | OBJECT | OBJECT ]
			//
			// 이렇게 만들어 줄 필요가 있음
			//
			// 그리고 링크드 리스트처럼 줄줄이 다음 위치를 가리키게 연결 해 두는 구조
			// 하단의 구문은 이를 위한 거
			//////////////////////////////////////////////////////////////////////////


			//////////////////////////////////////////////////////////////////////////
			// mFreeList는 uint8_t 배열
			// 즉 mFreeList 멤버변수에 담긴 값은 uint8_t* (uint8_t 배열의 시작 주소)
			//
			// uint8_t**는 mFreeList의 포인터를 담기 위함 (uint8_t*의 포인터)
			//////////////////////////////////////////////////////////////////////////
			
			uint8_t** ppCurr = reinterpret_cast<uint8_t**>(mFreeList) ;
			// mFreeList의 포인터(현재는 배열 시작위치)를 mFreeList의 이중 포인터 형태로 형 변환 해서 넣어둠

			for (int i=0 ; i<ALLOC_COUNT-1 ; ++i)
			{
				//////////////////////////////////////////////////////////////////////////
				/// OBJECT의 크기가 반드시 포인터 크기보다 커야 한다
				//
				// 추가 설명
				//
				// 배열 안에 미리 다음 위치의 포인터를 찍어 둠
				// 그러려면 OBJECT의 크기가 반드시 포인터 크기보다 커야 됨
				//
				// 그래야 OBJECT가 들어갈 공간에 미리 포인터를 찍어 놓을 수 있음
				//////////////////////////////////////////////////////////////////////////

				pNext += sizeof(TOBJECT) ;
				// pNext가 다음 OBJECT 칸을 가리킨다

				*ppCurr = pNext ;
				// 2중 포인터로 mFreeList의 위치를 ppCurr에 넣어두었으니
				// *ppCurr로 해당 위치에 pNext를 넣게 된다
				// (결과적으로 다음 위치 주소를 해당 배열에 넣어둠)

				ppCurr = reinterpret_cast<uint8_t**>(pNext) ;
				// 그리고 ppCurr은 다음 배열 위치의 주소로

				//////////////////////////////////////////////////////////////////////////
				// 그림으로 그려서 설명하면
				//
				// 초기 상태
				//
				//    pNext
				// [            |            |            |            | (후략)
				//   ppCurr
				//
				// 사실상 pNext와 ppCurr은 같음
				// 배열 안의 내용물로 배열의 다음 위치에 해당하는 주소 값을 넣기 위해서 이중포인터로 형변환 한 것 뿐
				//
				//
				// 1턴 진행하면
				//
				//                pNext는 여기로
				// [ pNext 저장 |            |            |            | (후략)
				//   └ *ppCurr = pNext ┘
				//
				// 이런 형태로 다음 위치 값을 또 넣고 넣고 for 루프 반복
				//////////////////////////////////////////////////////////////////////////
			}
			
			*ppCurr = 0 ; ///< 마지막은 0으로 표시
			mTotalAllocCount += ALLOC_COUNT ;
		}

		uint8_t* pAvailable = mFreeList ;
		// pAvailable은 mFreeList가 가리킬, 배열의 사용 가능한 현재 위치다!

		mFreeList = *reinterpret_cast<uint8_t**>(pAvailable) ;
		// 그리고 mFreeList는 현재 위치에 담겨 있는 주소값(다음 위치겠지)으로 갱신

		CRASH_ASSERT( mCurrentUseCount <= mTotalAllocCount );

		++mCurrentUseCount ;
		// 카운트 증가하고

		return pAvailable ;
		// 현재 위치 값을 리턴합시다.
	}

	static void	operator delete(void* obj)
	{
		CRASH_ASSERT( mCurrentUseCount > 0 ) ;

		--mCurrentUseCount ;
	
		*reinterpret_cast<uint8_t**>(obj) = mFreeList ;
		//////////////////////////////////////////////////////////////////////////
		// mFreeList는 다음에 쓸, 사용 가능한 배열의 현재 위치
		//
		// obj는 지금 지울 위치니까, 현재 지울 위치에 해당하는 값
		// 즉 *obj = mFreeList로, 현재 위치에 다음 위치 값을 저장한다
		//////////////////////////////////////////////////////////////////////////

		mFreeList = static_cast<uint8_t*>(obj) ;
		// 그리고 현재 위치를, 사용 가능한 배열의 현재 위치로 갱신
	}


private:
	static uint8_t*	mFreeList ;
	static int		mTotalAllocCount ; ///< for tracing
	static int		mCurrentUseCount ; ///< for tracing
};

//////////////////////////////////////////////////////////////////////////
// 정적 변수들에 대해서 초기화 필요
template <class TOBJECT, int ALLOC_COUNT>
uint8_t* ObjectPool<TOBJECT, ALLOC_COUNT>::mFreeList = nullptr ;

template <class TOBJECT, int ALLOC_COUNT>
int ObjectPool<TOBJECT, ALLOC_COUNT>::mTotalAllocCount = 0 ;

template <class TOBJECT, int ALLOC_COUNT>
int ObjectPool<TOBJECT, ALLOC_COUNT>::mCurrentUseCount = 0 ;
//////////////////////////////////////////////////////////////////////////