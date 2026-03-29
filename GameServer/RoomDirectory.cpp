#include "RoomDirectory.h"

RoomDirectory::RoomDirectory()
{
}

int RoomDirectory::CreateRoom(int hostPlayerId, int maxPlayers, RoomVisibility visibility)
{
    if (!IsValidPlayerCount(maxPlayers))
    {
        return 0;
    }

    if (FindRoomByPlayer(hostPlayerId) != nullptr)
    {
        return 0;
    }

    RoomEntry room{};
    room.roomId = GenerateRoomId();
    room.hostPlayerId = hostPlayerId;
    room.maxPlayers = maxPlayers;
    room.visibility = visibility;
    room.playerIds.push_back(hostPlayerId);

    mRooms.push_back(room);
    return room.roomId;
}

bool RoomDirectory::JoinRoom(int roomId, int playerId)
{
    RoomEntry* room = FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    if (FindRoomByPlayer(playerId) != nullptr)
    {
        return false;
    }

    if (ContainsPlayer(*room, playerId))
    {
        return false;
    }

    if (static_cast<int>(room->playerIds.size()) >= room->maxPlayers)
    {
        return false;
    }

    room->playerIds.push_back(playerId);
    return true;
}

bool RoomDirectory::RemovePlayerFromRoom(int playerId)
{
    for (auto roomIt = mRooms.begin(); roomIt != mRooms.end(); ++roomIt)
    {
        for (auto playerIt = roomIt->playerIds.begin(); playerIt != roomIt->playerIds.end(); ++playerIt)
        {
            if (*playerIt == playerId)
            {
                roomIt->playerIds.erase(playerIt);

                if (roomIt->playerIds.empty())
                {
                    mRooms.erase(roomIt);
                }

                return true;
            }
        }
    }

    return false;
}

RoomDirectory::RoomEntry* RoomDirectory::FindRoom(int roomId)
{
    for (RoomEntry& room : mRooms)
    {
        if (room.roomId == roomId)
        {
            return &room;
        }
    }

    return nullptr;
}

const RoomDirectory::RoomEntry* RoomDirectory::FindRoom(int roomId) const
{
    for (const RoomEntry& room : mRooms)
    {
        if (room.roomId == roomId)
        {
            return &room;
        }
    }

    return nullptr;
}

RoomDirectory::RoomEntry* RoomDirectory::FindRoomByPlayer(int playerId)
{
    for (RoomEntry& room : mRooms)
    {
        if (ContainsPlayer(room, playerId))
        {
            return &room;
        }
    }

    return nullptr;
}

const RoomDirectory::RoomEntry* RoomDirectory::FindRoomByPlayer(int playerId) const
{
    for (const RoomEntry& room : mRooms)
    {
        if (ContainsPlayer(room, playerId))
        {
            return &room;
        }
    }

    return nullptr;
}

bool RoomDirectory::StartRoomMatch(int roomId)
{
    RoomEntry* room = FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    if (static_cast<int>(room->playerIds.size()) < RuleBook::MIN_PLAYERS)
    {
        return false;
    }

    room->match.SetPlayers(room->playerIds);
    room->match.StartNextRound();
    return true;
}

std::vector<int> RoomDirectory::GetPlayersInRoom(int roomId) const
{
    const RoomEntry* room = FindRoom(roomId);
    if (room == nullptr)
    {
        return {};
    }

    return room->playerIds;
}

int RoomDirectory::GetRoomCount() const
{
    return static_cast<int>(mRooms.size());
}

//int RoomDirectory::GenerateRoomId()
//{
//    return mNextRoomId++;
//}

int RoomDirectory::GenerateRoomId()
{
    int candidate = 1;

    while (true)
    {
        bool used = false;

        for (const RoomEntry& room : mRooms)
        {
            if (room.roomId == candidate)
            {
                used = true;
                break;
            }
        }

        if (!used)
        {
            return candidate;
        }

        ++candidate;
    }
}

bool RoomDirectory::IsValidPlayerCount(int maxPlayers) const
{
    return maxPlayers >= RuleBook::MIN_PLAYERS &&
        maxPlayers <= RuleBook::MAX_PLAYERS;
}

bool RoomDirectory::ContainsPlayer(const RoomEntry& room, int playerId) const
{
    for (int existingPlayerId : room.playerIds)
    {
        if (existingPlayerId == playerId)
        {
            return true;
        }
    }

    return false;
}

bool RoomDirectory::IsHost(int roomId, int playerId) const
{
    const RoomEntry* room = FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    return room->hostPlayerId == playerId;
}

bool RoomDirectory::CloseRoom(int roomId)
{
    for (auto it = mRooms.begin(); it != mRooms.end(); ++it)
    {
        if (it->roomId == roomId)
        {
            mRooms.erase(it);
            return true;
        }
    }

    return false;
}