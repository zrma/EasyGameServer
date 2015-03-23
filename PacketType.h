﻿#pragma once

#define MAX_CHAT_LEN	256

#define MAX_NAME_LEN	30
#define MAX_COMMENT_LEN	40

enum PacketTypes
{
	PKT_NONE	= 0,
	
	//////////////////////////////////////////////////////////////////////////
	// Login Packet
	PKT_CS_LOGIN	= 1,
	// Client to Server (서버쪽에서 핸들링)
	PKT_SC_LOGIN	= 2,
	// Server to Client (클라쪽에서 핸들링)
	
	//////////////////////////////////////////////////////////////////////////
	// Chat Packet
	PKT_CS_CHAT		= 11,
	// Client to Server (서버쪽에서 핸들링)
	PKT_SC_CHAT		= 12,
	// Server to Client (클라쪽에서 핸들링)
	
	PKT_CS_MOVE		= 21,
	PKT_SC_MOVE		= 22,

	PKT_MAX			= 1024
} ;

#pragma pack(push, 1)
//////////////////////////////////////////////////////////////////////////
// 바이트 정렬을 1바이트로
//
// 2학기 Windows OS 실습 시간에 다룬 주제
// http://blog.naver.com/pjbmylove/110177847013 참조
//////////////////////////////////////////////////////////////////////////

struct PacketHeader
{
	PacketHeader() : mSize(0), mType(PKT_NONE) 	{}
	short mSize ;
	short mType ;
} ;


//////////////////////////////////////////////////////////////////////////
// 로그인 요청 패킷
// Client to Server
//////////////////////////////////////////////////////////////////////////
struct LoginRequest : public PacketHeader
{
	LoginRequest()
	{
		mSize = sizeof(LoginRequest) ;
		mType = PKT_CS_LOGIN ;
		mPlayerId = -1 ;
	}

	int	mPlayerId ;
} ;

//////////////////////////////////////////////////////////////////////////
// 로그인 결과 패킷
// Server to Client
//////////////////////////////////////////////////////////////////////////
struct LoginResult : public PacketHeader
{
	LoginResult()
	{
		mSize = sizeof(LoginResult) ;
		mType = PKT_SC_LOGIN ;
		mPlayerId = -1 ;
		memset(mName, 0, MAX_NAME_LEN) ;
	}

	int		mPlayerId ;
	double	mPosX ;
	double	mPosY ;
	char	mName[MAX_NAME_LEN] ;
	//////////////////////////////////////////////////////////////////////////
	// 로그인에 성공하면 로그인 결과값으로 id, position, name 등 정보를 전송
	//////////////////////////////////////////////////////////////////////////
} ;

//////////////////////////////////////////////////////////////////////////
// Broadcast 요청 패킷
// Client to Server
//////////////////////////////////////////////////////////////////////////
struct ChatBroadcastRequest : public PacketHeader
{
	ChatBroadcastRequest()
	{
		mSize = sizeof(ChatBroadcastRequest) ;
		mType = PKT_CS_CHAT ;
		mPlayerId = -1 ;
	
		memset(mChat, 0, MAX_CHAT_LEN) ;
	}

	int	mPlayerId ;
	char mChat[MAX_CHAT_LEN];
	//////////////////////////////////////////////////////////////////////////
	// 클라쪽에서 자신의 id와 채팅 내용 문자열을 담아서 보냄
	//////////////////////////////////////////////////////////////////////////
} ;

//////////////////////////////////////////////////////////////////////////
// Broadcast 결과 패킷
// Server to Client
//////////////////////////////////////////////////////////////////////////
struct ChatBroadcastResult : public PacketHeader
{
	ChatBroadcastResult()
	{
		mSize = sizeof(ChatBroadcastResult) ;
		mType = PKT_SC_CHAT ;
		mPlayerId = -1 ;
		
		memset(mName, 0, MAX_NAME_LEN) ;
		memset(mChat, 0, MAX_CHAT_LEN) ;
	}
	
	int	mPlayerId ;
	char mName[MAX_NAME_LEN] ;
	char mChat[MAX_CHAT_LEN] ;
	//////////////////////////////////////////////////////////////////////////
	// id는 수신 받을 타겟 id
	// mName은 Broadcast를 요청했던 클라의 이름
	// mChat는 채팅 내용
	//////////////////////////////////////////////////////////////////////////
} ;

struct MoveRequest : public PacketHeader
{
	MoveRequest()
	{
		mSize = sizeof( MoveRequest );
		mType = PKT_CS_MOVE;
		mPlayerId = -1;
		mPosX = 0;
		mPosY = 0;
	}

	int		mPlayerId;
	float	mPosX;
	float	mPosY;
};

struct MoveBroadcastResult : public PacketHeader
{
	MoveBroadcastResult()
	{
		mSize = sizeof( MoveBroadcastResult );
		mType = PKT_SC_MOVE;
		mPlayerId = -1;
		mPosX = 0;
		mPosY = 0;
	}

	int		mPlayerId;
	float	mPosX;
	float	mPosY;
};

#pragma pack(pop)
//////////////////////////////////////////////////////////////////////////
// pragma pack에서 push 한 후에 pop을 안 할 경우 문제가 생길 소지가 있음
//
// http://blog.naver.com/gal_yong/30025363034 참조
//////////////////////////////////////////////////////////////////////////