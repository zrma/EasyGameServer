#include "stdafx.h"
#include "DatabaseJobContext.h"
#include "SQLStatement.h"
#include "DbHelper.h"

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
