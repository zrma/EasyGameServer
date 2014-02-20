#include "stdafx.h"
#include "DbHelper.h"
#include "sqlite3.h"
#include "Exception.h"

sqlite3* DbHelper::mSqlite = NULL ;
// 전역 변수

DbHelper::DbHelper(const char* sql) 
	: mResult(NULL), mResultColCount(0), mBindColCount(0), mResultCurrentCol(0)
{
	char* errMsg = NULL ;

	//////////////////////////////////////////////////////////////////////////
	// http://nickeys.tistory.com/entry/SQLite3 참조
	//
	// sqlite3_prepare vs sqlite3_prepare_v2
	//
	// 이 루틴은 SQL 문장을 prepared statement 객체로 전환하고 그 객체에 대한 포인터를 반환한다.
	// -> mResult
	//
	// 이 인터페이스는 앞에서 sqlite3_open()을 호출해서 만들어진 DB 연결 객체의 포인터와
	// SQL 문장을 포함한 문자열을 인자로 받는다.
	// -> mSqlite, sql
	// 
	// 이 API는 실재로 SQL 문장을 평가하는 것이 아니고,
	// 단지 SQL 문장에 대한 평가를 준비하는 단계이다.
	//
	// 새로운 응용 프로그램에서 sqlite3_prepare()의 사용은 추천되지 않는다.
	// 대신 sqlite3_prepare_v2()를 사용하는 것이 낫다.
	//////////////////////////////////////////////////////////////////////////
	//	SQLITE_API int sqlite3_prepare_v2(
	//			
	//		sqlite3 *db,              /* Database handle.							= mSqlite		*/
	//		const char *zSql,         /* UTF-8 encoded SQL statement.				= sql 쿼리문	*/
	//		int nBytes,               /* Length of zSql in bytes.					= -1			*/
	//		sqlite3_stmt **ppStmt,    /* OUT: A pointer to the prepared statement	= mResult 주소	*/
	//		const char **pzTail       /* OUT: End of parsed string					= NULL			*/
	//
	//		)
	//	{
	//		int rc;
	//		rc = sqlite3LockAndPrepare(db,zSql, nBytes, 1, 0, ppStmt, pzTail);
	//
	//		// 여기에서 sqlite_3_prepare와 v2의 차이는 Bytes 위의 값이 1, 0인데
	//		// 해당 인자 값은 safeSQLFlag이다. 그래서 v2를 권장하는 듯?
	//
	//		assert( rc==SQLITE_OK || ppStmt==0 || *ppStmt==0 );  /* VERIFY: F13021 */
	//		return rc;
	//	}
	//
	// http://www.cocoadev.co.kr/212 참조
	//////////////////////////////////////////////////////////////////////////
	if ( sqlite3_prepare_v2(mSqlite, sql, -1, &mResult, NULL) != SQLITE_OK )
	{
		printf("DbHelper Query [%s] Prepare failed: %s\n", sql, sqlite3_errmsg(mSqlite)) ;
		CRASH_ASSERT(false) ;
	}
}

DbHelper::~DbHelper()
{
	sqlite3_finalize(mResult) ;
	// 전달 인자로 쿼리문이 prepared statment 객체로 전환 된 포인터를 넣음 = mResult

	//////////////////////////////////////////////////////////////////////////
	// 이 루틴은 sqlite3_prepare()의 호출로 부터 만들어진 prepared statement를 파괴한다.
	// 모든 prepared statement는 메모리 누수를 막기 위해서 이 루틴을 호출해서 파괴되어야 한다.
	//
	// DatabaseJobcntext.cpp 참조
	//
	// DB 작업을 요청하기 위해 작업 종류별로 Context를 생성해서 DB Job Manager의 큐에 넣게 된다.
	// 그러면 EasyServer.cpp에서 생성한 DB 핸들링 스레드가 무한루프 돌면서 큐에서 꺼내서 작업 처리.
	//
	// 작업 처리 = OnExecute() 실행
	//
	// 각 Context의 OnExecute()에는 SQL 문장(SQL 쿼리문)을 갖고 함수 내의 지역변수(스택을 이용)로
	// DbHelper를 생성해서 DB 관련 일을 처리하게 된다.
	//
	// 함수 호출이 종료 되면 스택이 비워지게 되므로 DbHelper의 소멸자를 호출하게 되고,
	// 여기서 sqlite3_finalize()를 호출하게 된다
	//
	//
	// 생성자 - sqlite3_prepare_v2()
	// 소멸자 - sqlite3_finalize()
	//
	// 짝이 맞게 되어 있음
	//////////////////////////////////////////////////////////////////////////
}

bool DbHelper::Initialize(const char* connInfoStr)
{
	int result = sqlite3_open(connInfoStr, &mSqlite) ;
	//////////////////////////////////////////////////////////////////////////
	// http://nickeys.tistory.com/entry/SQLite3 참조
	// 
	// sqlite3_open()
	// 이 루틴은 SQLite DB파일을 열고(connInfoStr 안에 DB파일 주소 들어있음)
	// DB 연결 객체를 반환 한다. -> mSqlite
	//
	// 이 루틴은 응용프로그램이 빈번하게 제일 먼저 호출 하는데, 대부분의 SQLite API들의 선행 조건이기 때문이다.
	// 많은 SQLite 인터페이스들은 첫 번째 인자로 DB 연결에 대한 포인터를 요구한다.
	// 이 루틴은 DB 객체의 생성자 역할을 한다고 할 수 있겠다.
	//
	// 따라서 EasyServer.cpp 에서 _tmain()에서 초기 기동 시에
	// DbHelper::Initialize(DB_CONN_STR) 해줌. 실패 할 시에 서버 종료
	//////////////////////////////////////////////////////////////////////////

	if ( mSqlite == NULL || result != SQLITE_OK )
	{
		printf("DbHelper::Initialize Error\n") ;
		return false ;
	}

	return true ;
}


void DbHelper::Finalize()
{
	if (mSqlite)
		sqlite3_close(mSqlite) ;
	//////////////////////////////////////////////////////////////////////////
	// 이 루틴은 이전에 sqlite3_open()을 호출함으로써 만들어진 DB연결을 닫는다.
	// 모든 해당 연결에 관련된 prepared statement들은 해당 연결이 닫히기 전에 종결 되어야 한다.
	//
	// 정리 -> EasyServer.cpp 에서 서버 종료 전에 한 번 실행 함.
	// DB 파일 열려 있으므로 닫는다.
	//////////////////////////////////////////////////////////////////////////
}

// 이 함수는 실제로 사용되지 않았음
bool DbHelper::Execute(const char* format, ...)
{
	if (!format)
		return false ;

	if (!mSqlite)
		return false ;

	char sqlQuery[1024] = {0, } ;

	//////////////////////////////////////////////////////////////////////////
	//
	// 가변적인 개수의 매개인자를 갖는 함수를 만들기 위해서는 stdarg.h 헤더를
	// 포함해야 하는데, sqlite3.h 에서 불러오고 있음
	//
	// http://mndd.tistory.com/102 참고

	va_list ap ;
	// 포인터의 데이터형

	va_start(ap, format) ;
	// 인수 목록 초기화 매크로

	int res = vsnprintf_s(sqlQuery, 1024, format, ap) ; 

	va_end(ap) ;
	// 모든 인수를 받아들이고 나서 정리 동작을 수행하는데 사용되는 매크로
	//////////////////////////////////////////////////////////////////////////

	if (-1 == res)
		return false ;
	
	char* errMsg = NULL ;
	
	///FYI: 사실 실무에서는 (SQL Injection 때문에) 이렇게 쿼리를 직접 넣지 않고
	///파라미터별로 일일이 BIND한다. (BindParamXXX 멤버함수 참고)
	if (sqlite3_exec(mSqlite, sqlQuery, NULL, NULL, &errMsg) != SQLITE_OK)
	{
		printf("SQL [%s] ERROR [%s] \n", sqlQuery, errMsg) ;
		return false ;
	}
	//////////////////////////////////////////////////////////////////////////
	//	SQLITE_API int sqlite3_exec(
	//		sqlite3*,                                  /* An open database			= mSqlite				*/
	//		const char *sql,                           /* SQL to be evaluated		= sqlQuery : 쿼리문		*/
	//		int (*callback)(void*,int,char**,char**),  /* Callback function			= NULL					*/
	//		void *,                                    /* 1st argument to callback	= NULL					*/
	//		char **errmsg                              /* Error msg written here	= 에러 메세지 담을 주소	*/
	//	);
	//////////////////////////////////////////////////////////////////////////

	return true ;
}

//////////////////////////////////////////////////////////////////////////
// mBindColCount는 생성자에서 0으로 초기화 되고, Bind 할 때마다 1씩 증가
//
// DB 쿼리문 ~~~ values(?,?,?,?) 에서 n번째에 데이터를 넣으라고 컨트롤 하는 것을
// mBindColCount 로 컨트롤 하기 위해 ++ 함
//
// http://notpeelbean.tistory.com/121 참고
//////////////////////////////////////////////////////////////////////////
bool DbHelper::BindParamInt(int param)
{
	//////////////////////////////////////////////////////////////////////////
	// 1. 쿼리문이 prepared statment 객체로 전환 된 포인터 = mResult
	// 
	// 2. mBindColCount 번째 칸에
	//
	// 3. param 값을 넣어라
	//////////////////////////////////////////////////////////////////////////
	if ( sqlite3_bind_int(mResult, ++mBindColCount, param) != SQLITE_OK )
	{
		printf("DbHelper Bind Int failed: %s\n", sqlite3_errmsg(mSqlite)) ;
		return false ;
	}

	return true ;
}

bool DbHelper::BindParamDouble(double param)
{
	if ( sqlite3_bind_double(mResult, ++mBindColCount, param) != SQLITE_OK )
	{
		printf("DbHelper Bind Double failed: %s\n", sqlite3_errmsg(mSqlite)) ;
		return false ;
	}

	return true ;
}

bool DbHelper::BindParamText(const char* text, int count)
{
	//////////////////////////////////////////////////////////////////////////
	// 네번째 인자는 세번째 데이터의 크기를 정한다
	// -1(음수)를 넣으면 전체 데이터를 넣는다.
	//
	// 다섯번째 인자는 특별한 인자 이다.
	// 들어가는 값으로 SQLITE_STATIC 과 SQLITE_TRANSIENT가 있다.
	//
	// SQLITE_STATIC의 경우 바인딩 되는 변수를 static 변수(free가 될 일이 없는)로 사용한다는 의미
	// 변수가 중간에 변경되거나 메모리가 해제 되면 문제가 발생할 수 있다.
	//
	// SQLITE_TRANSIENT는 바인딩 변수가중간에 변경이 될 수도 있기에 해당 변수값을 복사하여 사용한다.
	// 중간에 변수가 변경이 되어도 복사한 값으로 사용되기에 문제 없다.
	// 다만 복사과정이 들어가기에 안전하나 SQLITE_STATIC 보다는 속도가 느리다
	//
	// http://neptjuno.tistory.com/41 참조
	//////////////////////////////////////////////////////////////////////////
	if ( sqlite3_bind_text(mResult, ++mBindColCount, text, strlen(text), NULL) != SQLITE_OK )
	{
		printf("DbHelper Bind Text failed: %s\n", sqlite3_errmsg(mSqlite)) ;
		return false ;
	}

	return true ;
}

RESULT_TYPE DbHelper::FetchRow()
{
	int result = sqlite3_step(mResult) ;
	//////////////////////////////////////////////////////////////////////////
	// http://nickeys.tistory.com/entry/SQLite3 참조
	//
	// sqlite3_step() SQL 문장을 prepared statement 객체로 전환한 mResult를 매개인자로 넣어서
	// 실제 쿼리문에 대해서 평가함
	//
	// sqlite3_prepare_v2()	: SQL 명령문 형식에 대한 평가
	// sqlite3_step()		: 실제 명령문에 인자를 bind 한 이후 적용에 대한 평가
	//////////////////////////////////////////////////////////////////////////
	
	if ( result != SQLITE_ROW && result != SQLITE_DONE )
	{
		printf("DbHelper FetchRow failed: %s\n", sqlite3_errmsg(mSqlite)) ;
		return RESULT_ERROR ;
	}

	/// 결과셋으로 얻어올 데이터가 없다. (그냥 쿼리 실행만 된 것)
	if ( result == SQLITE_DONE )
		return RESULT_DONE ;
	// 결과를 반환하지 않는 문장들(INSERT, UPDATE, DELETE)은 sqlite3_step()을 한번 호출하면 완료

	//////////////////////////////////////////////////////////////////////////
	// 결과를 반환 할 경우
	// sqlite3_column() 으로 결과 집합의 한 열 반환
	//
	// sqlite3_column_blob()
	// sqlite3_column_bytes()
	// sqlite3_column_bytes16()
	// sqlite3_column_count()
	// sqlite3_column_double()
	// sqlite3_column_int()
	// sqlite3_column_int64()
	// sqlite3_column_text()
	// sqlite3_column_text16()
	// sqlite3_column_type()
	// sqlite3_column_value()
	//
	// http://www.sqlite.org/c3ref/data_count.html
	// http://www.sqlite.org/c3ref/column_count.html 참고
	//
	//
	// sqlite3_column_count(mResult)
	//
	// 쿼리문 prepared statment 객체 (mResult) 의 평가 결과에 맞는 행(row)의 
	// 열(column)의 개수 리턴
	//////////////////////////////////////////////////////////////////////////
	mResultColCount = sqlite3_column_count(mResult) ;

	mResultCurrentCol = 0 ;
	// 결과를 반환해야 하므로 GetResultParam() 으로 얻어오려면 현재 열을 카운트 해야 됨. 초기화

	return RESULT_ROW ;
}

int DbHelper::GetResultParamInt()
{
	CRASH_ASSERT( mResultCurrentCol < mResultColCount ) ;
	// 최대 열 수를 초과하면 문제가 있는 거
	// 최대 열이 10개인데 GetResult를 11번 호출했다던가...
	
	return sqlite3_column_int(mResult, mResultCurrentCol++) ;
	// mResultCurrentCol로 현재 열 수를 카운트하자
}

double DbHelper::GetResultParamDouble()
{
	CRASH_ASSERT( mResultCurrentCol < mResultColCount ) ;

	return sqlite3_column_double(mResult, mResultCurrentCol++) ;
}

const unsigned char* DbHelper::GetResultParamText()
{
	CRASH_ASSERT( mResultCurrentCol < mResultColCount ) ;
	
	return sqlite3_column_text(mResult, mResultCurrentCol++) ;
}