#include "stdafx.h"
#include "ClientSession.h"
#include "..\..\PacketType.h"
#include "ClientManager.h"


bool ClientSession::OnConnect(SOCKADDR_IN* addr)
{
	memcpy(&mClientAddr, addr, sizeof(SOCKADDR_IN)) ;

	/// ������ �ͺ���ŷ���� �ٲٰ�
	u_long arg = 1 ;
	::ioctlsocket(mSocket, FIONBIO, &arg) ;

	/// nagle �˰����� ����
	int opt = 1 ;
	::setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)) ;

	printf("[DEBUG] Client Connected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port)) ;
	
	mConnected = true ;

	return PostRecv() ;
}

bool ClientSession::PostRecv()
{
	if ( !IsConnected() )
		return false ;

	DWORD recvbytes = 0 ;
	DWORD flags = 0 ;
	WSABUF buf ;
	buf.len = (ULONG)mRecvBuffer.GetFreeSpaceSize() ;
	buf.buf = (char*)mRecvBuffer.GetBuffer() ;

	memset(&mOverlappedRecv, 0, sizeof(OverlappedIO)) ;
	mOverlappedRecv.mObject = this ;

	/// �񵿱� ����� ����
	if ( SOCKET_ERROR == WSARecv(mSocket, &buf, 1, &recvbytes, &flags, &mOverlappedRecv, RecvCompletion) )
	{
		if ( WSAGetLastError() != WSA_IO_PENDING )
			return false ;
	}

	return true ;
}

void ClientSession::Disconnect()
{
	if ( !IsConnected() )
		return ;

	printf("[DEBUG] Client Disconnected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port)) ;

	::shutdown(mSocket, SD_BOTH) ;
	::closesocket(mSocket) ;

	mConnected = false ;
}


void ClientSession::OnRead(size_t len)
{
	mRecvBuffer.Commit(len) ;

	/// ��Ŷ �Ľ��ϰ� ó��
	while ( true )
	{
		/// ��Ŷ ��� ũ�� ��ŭ �о�ͺ���
		PacketHeader header ;
		if ( false == mRecvBuffer.Peek((char*)&header, sizeof(PacketHeader)) )
			return ;

		/// ��Ŷ �ϼ��� �Ǵ°�? 
		if ( mRecvBuffer.GetStoredSize() < (header.mSize - sizeof(PacketHeader)) )
			return ;

		/// ��Ŷ �ڵ鸵
		switch ( header.mType )
		{
		case PKT_CS_PING:
			{
				TestPing inPacket ;
				mRecvBuffer.Read((char*)&inPacket, header.mSize) ;
				
				TestPong outPacket ;
				sprintf_s(outPacket.mData, "SC_PONG Broadcast fromClient[%d]: %.4f, %.4f, %.4f \n", inPacket.mPlayerId, inPacket.mPosX, inPacket.mPosY, inPacket.mPosZ ) ;
				outPacket.mResult = true ;
				outPacket.mPlayerId = mClientId ;
				
				/// CS_PING ���� ��� �������� Ŭ�󿡰� ����ϴ� �� �׽�Ʈ
				if ( !Broadcast(&outPacket) )
					return ;
			}
			break ;

		case PKT_CS_PING2:
			{
				TestPing2 inPacket ;
				mRecvBuffer.Read((char*)&inPacket, header.mSize) ;

				//printf("[CLIENT] %s \n", inPacket.mData ) ;
				
				TestPong2 outPacket ;
				outPacket.mPlayerId = mClientId ;
				outPacket.mPosX = 0.1111f + inPacket.mPosX ;
				outPacket.mPosY = 0.2222f + inPacket.mPosY ;
				outPacket.mPosZ = 0.3333f + inPacket.mPosZ ;
				outPacket.mResult = true ;

				/// CS_PING2 ���� ��ġ�� �� �ٲ㼭 reply �ϴ� �� �׽�Ʈ
				if ( !Send(&outPacket) )
					return ;
 
			}
			break ;

		default:
			{
				/// ���� ������ �̻��� ��Ŷ �����Ŵ�.
				Disconnect() ;
				return ;
			}
			break ;
		}
	}
}

bool ClientSession::Send(PacketHeader* pkt)
{
	if ( !IsConnected() )
		return false ;

	/// ���� �뷮 ������ ���� �������
	if ( false == mSendBuffer.Write((char*)pkt, pkt->mSize) )
	{
		Disconnect() ;
		return false ;
	}

	/// ���� �����Ͱ� �ִ��� �˻�
	if ( mSendBuffer.GetContiguiousBytes() == 0 )
	{
		/// ������� write �ߴµ�, �����Ͱ� ���ٸ� ���� �߸��� ��
		assert(false) ;
		Disconnect() ;
		return false ;
	}
		
	DWORD sendbytes = 0 ;
	DWORD flags = 0 ;

	WSABUF buf ;
	buf.len = (ULONG)mSendBuffer.GetContiguiousBytes() ;
	buf.buf = (char*)mSendBuffer.GetBufferStart() ;
	
	memset(&mOverlappedSend, 0, sizeof(OverlappedIO)) ;
	mOverlappedSend.mObject = this ;

	// �񵿱� ����� ����
	if ( SOCKET_ERROR == WSASend(mSocket, &buf, 1, &sendbytes, flags, &mOverlappedSend, SendCompletion) )
	{
		if ( WSAGetLastError() != WSA_IO_PENDING )
			return false ;
	}

	return true ;
}

void ClientSession::OnWriteComplete(size_t len)
{
	/// ������ �Ϸ��� �����ʹ� ���ۿ��� ����
	mSendBuffer.Remove(len) ;

	/// ��? �� ���� ��쵵 �ֳ�? (Ŀ���� send queue�� ��á�ų�, Send Completion������ �� send �� ���?)
	if ( mSendBuffer.GetContiguiousBytes() > 0 )
	{
		assert(false) ;
	}

}

bool ClientSession::Broadcast(PacketHeader* pkt)
{
	if ( !Send(pkt) )
		return false ;

	if ( !IsConnected() )
		return false ;

	GClientManager->BroadcastPacket(this, pkt) ;

	return true ;
}

void ClientSession::OnTick()
{
	/// Ŭ�󺰷� �ֱ������� �ؾߵ� ���� ���⿡
	//printf("DEBUG: Client[%d] OnTick %u \n", mClientId, GetTickCount()) ;
}

///////////////////////////////////////////////////////////

void CALLBACK RecvCompletion(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	ClientSession* fromClient = static_cast<OverlappedIO*>(lpOverlapped)->mObject ;
	
	if ( !fromClient->IsConnected() )
		return ;

	/// ���� �߻��� �ش� ���� ����
	if ( dwError || cbTransferred == 0 )
	{
		fromClient->Disconnect() ;
		return ;
	}

	/// ���� ������ ó��
	fromClient->OnRead(cbTransferred) ;

	/// �ٽ� �ޱ�
	if ( false == fromClient->PostRecv() )
	{
		fromClient->Disconnect() ;
		return ;
	}
}


void CALLBACK SendCompletion(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags)
{
	ClientSession* fromClient = static_cast<OverlappedIO*>(lpOverlapped)->mObject ;

	if ( !fromClient->IsConnected() )
		return ;

	/// ���� �߻��� �ش� ���� ����
	if ( dwError || cbTransferred == 0 )
	{
		fromClient->Disconnect() ;
		return ;
	}

	fromClient->OnWriteComplete(cbTransferred) ;

}
