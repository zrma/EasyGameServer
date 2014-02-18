#include "stdafx.h"
#include "DatabaseJobContext.h"
#include "SQLStatement.h"
#include "DbHelper.h"

//////////////////////////////////////////////////////////////////////////
// 1. Dbhelper() 생성자를 호출하면
// 인자로 전달한 SQL 쿼리문을 갖고 sqlite3_prepare_v2() 를 호출
//
// 2. BindParam으로 파라메터 별로 바인드
//
// 3. FetchRow() 함수로 sqlite3_step() 를 호출
//
// 4. 그 결과가 있는 쿼리문에 대해서는 GetResultParam()으로 얻어오면 된다.
//////////////////////////////////////////////////////////////////////////

bool LoadPlayerDataContext::OnExecute()
{
	//////////////////////////////////////////////////////////////////////////
	// const char* SQL_SelectTest = "SELECT name, pos_x, pos_y, pos_z FROM players WHERE pid=?" ;
	//////////////////////////////////////////////////////////////////////////
	DbHelper dbhelper(SQL_SelectTest) ;
	dbhelper.BindParamInt(mPlayerId) ;

	/// 데이터가 없네?
	if ( RESULT_ROW != dbhelper.FetchRow() )
		return false ;


	const unsigned char* name = dbhelper.GetResultParamText() ;
	mPosX = dbhelper.GetResultParamDouble() ;
	mPosY = dbhelper.GetResultParamDouble() ;
	mPosZ = dbhelper.GetResultParamDouble() ;

	strcpy_s(mPlayerName, (char*)name) ;


	return true ;
}

bool CreatePlayerDataContext::OnExecute()
{
	//////////////////////////////////////////////////////////////////////////
	// const char* SQL_InsertTest = "INSERT INTO players VALUES(?, ?, ?, ?, ?, ?)" ;
	//////////////////////////////////////////////////////////////////////////
	DbHelper dbhelper(SQL_InsertTest) ;

	dbhelper.BindParamInt(mPlayerId) ;
	dbhelper.BindParamText(mPlayerName, strlen(mPlayerName)) ;
	dbhelper.BindParamDouble(mPosX) ;
	dbhelper.BindParamDouble(mPosY) ;
	dbhelper.BindParamDouble(mPosZ) ;
	dbhelper.BindParamText(mComment, strlen(mComment)) ;

	if ( RESULT_ERROR == dbhelper.FetchRow() )
		return false ;

	return true ;
}

bool DeletePlayerDataContext::OnExecute()
{
	//////////////////////////////////////////////////////////////////////////
	// const char* SQL_DeleteTest = "DELETE FROM players WHERE pid=?" ;
	//////////////////////////////////////////////////////////////////////////
	DbHelper dbhelper(SQL_DeleteTest) ;

	dbhelper.BindParamInt(mPlayerId) ;

	if ( RESULT_ERROR == dbhelper.FetchRow() )
		return false ;

	return true ;

}

bool UpdatePlayerDataContext::OnExecute()
{
	//////////////////////////////////////////////////////////////////////////
	// const char* SQL_UpdateTest = "UPDATE players SET pos_x=?, pos_y=?, pos_z=?, comment=? WHERE pid=?" ;
	//////////////////////////////////////////////////////////////////////////
	DbHelper dbhelper(SQL_UpdateTest) ;

	dbhelper.BindParamDouble(mPosX) ;
	dbhelper.BindParamDouble(mPosY) ;
	dbhelper.BindParamDouble(mPosZ) ;
	dbhelper.BindParamText(mComment, strlen(mComment)) ;
	dbhelper.BindParamInt(mPlayerId) ;

	if ( RESULT_ERROR == dbhelper.FetchRow() )
		return false ;

	return true ;

}
