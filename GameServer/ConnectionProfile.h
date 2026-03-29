#pragma once

#include <string>

class ConnectionProfile
{
public:
    ConnectionProfile() = default;
    ConnectionProfile(int playerId, const std::string& playerName);

    int GetPlayerId() const;
    const std::string& GetPlayerName() const;

    void SetPlayerName(const std::string& playerName);

    int GetRoomId() const;
    void SetRoomId(int roomId);
    void ClearRoomId();

    bool IsConnected() const;
    void SetConnected(bool connected);

    bool IsReady() const;
    void SetReady(bool ready);

    bool WantsPlayAgain() const;
    void SetPlayAgain(bool playAgain);

private:
    int mPlayerId = 0;
    std::string mPlayerName = "Player";
    int mRoomId = 0;
    bool mConnected = true;
    bool mReady = false;
    bool mPlayAgain = false;
};