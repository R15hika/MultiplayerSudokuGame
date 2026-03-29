#include "ConnectionProfile.h"

ConnectionProfile::ConnectionProfile(int playerId, const std::string& playerName)
    : mPlayerId(playerId)
{
    if (!playerName.empty())
    {
        mPlayerName = playerName;
    }
}

int ConnectionProfile::GetPlayerId() const
{
    return mPlayerId;
}

const std::string& ConnectionProfile::GetPlayerName() const
{
    return mPlayerName;
}

void ConnectionProfile::SetPlayerName(const std::string& playerName)
{
    if (!playerName.empty())
    {
        mPlayerName = playerName;
    }
}

int ConnectionProfile::GetRoomId() const
{
    return mRoomId;
}

void ConnectionProfile::SetRoomId(int roomId)
{
    mRoomId = roomId;
}

void ConnectionProfile::ClearRoomId()
{
    mRoomId = 0;
}

bool ConnectionProfile::IsConnected() const
{
    return mConnected;
}

void ConnectionProfile::SetConnected(bool connected)
{
    mConnected = connected;
}

bool ConnectionProfile::IsReady() const
{
    return mReady;
}

void ConnectionProfile::SetReady(bool ready)
{
    mReady = ready;
}

bool ConnectionProfile::WantsPlayAgain() const
{
    return mPlayAgain;
}

void ConnectionProfile::SetPlayAgain(bool playAgain)
{
    mPlayAgain = playAgain;
}