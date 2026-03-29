#pragma once

#include <vector>

#include "../Shared/SharedModels.h"
#include "LiveMatch.h"

class RoomDirectory
{
public:
    struct RoomEntry
    {
        int roomId = 0;
        int hostPlayerId = 0;
        int maxPlayers = RuleBook::MIN_PLAYERS;
        RoomVisibility visibility = RoomVisibility::Private;
        std::vector<int> playerIds;
        LiveMatch match;
    };

public:
    RoomDirectory();

    int CreateRoom(int hostPlayerId, int maxPlayers, RoomVisibility visibility);
    bool JoinRoom(int roomId, int playerId);
    bool RemovePlayerFromRoom(int playerId);

    RoomEntry* FindRoom(int roomId);
    const RoomEntry* FindRoom(int roomId) const;

    RoomEntry* FindRoomByPlayer(int playerId);
    const RoomEntry* FindRoomByPlayer(int playerId) const;

    bool StartRoomMatch(int roomId);

    std::vector<int> GetPlayersInRoom(int roomId) const;
    int GetRoomCount() const;

    bool IsHost(int roomId, int playerId) const;
    bool CloseRoom(int roomId);

private:
    int GenerateRoomId();
    bool IsValidPlayerCount(int maxPlayers) const;
    bool ContainsPlayer(const RoomEntry& room, int playerId) const;

private:
    std::vector<RoomEntry> mRooms;
    int mNextRoomId = 1;
};