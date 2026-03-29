#pragma once

#include <string>

#include "PlayerViewport.h"
#include "ServerBridge.h"

class StartMenu
{
public:
    void SetPlayerName(const std::string& name);
    const std::string& GetPlayerName() const;

    void SetPassword(const std::string& password);
    const std::string& GetPassword() const;

    void SignUp(ServerBridge& bridge);
    void SignIn(ServerBridge& bridge);

    void SetDesiredPlayerCount(int count);
    int GetDesiredPlayerCount() const;

    void SetTargetRoomId(int roomId);
    int GetTargetRoomId() const;

    void SetRoomVisibility(RoomVisibility visibility);
    RoomVisibility GetRoomVisibility() const;

    void CreateRoom(PlayerViewport& viewport, ServerBridge& bridge);
    void JoinRoom(PlayerViewport& viewport, ServerBridge& bridge);

private:
    std::string mPlayerName = "Player";
    std::string mPassword;
    int mDesiredPlayerCount = RuleBook::MIN_PLAYERS;
    int mTargetRoomId = 0;
    RoomVisibility mRoomVisibility = RoomVisibility::Private;
    std::vector<MatchHistoryRow> mHistoryRows;
};