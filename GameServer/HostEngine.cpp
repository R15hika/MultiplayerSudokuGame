#include "HostEngine.h"

HostEngine::HostEngine()
{
}

int HostEngine::RegisterPlayer(const std::string& playerName)
{
    int newPlayerId = GeneratePlayerId();
    mPlayers.emplace_back(newPlayerId, playerName);
    return newPlayerId;
}

int HostEngine::CreateRoomForPlayer(int playerId, int maxPlayers, RoomVisibility visibility)
{
    ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return 0;
    }

    if (player->GetRoomId() != 0)
    {
        return 0;
    }

    int roomId = mRooms.CreateRoom(playerId, maxPlayers, visibility);
    if (roomId != 0)
    {
        player->SetRoomId(roomId);
        player->SetReady(false);
        player->SetPlayAgain(false);
    }

    return roomId;
}

bool HostEngine::JoinRoomForPlayer(int playerId, int roomId)
{
    ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return false;
    }

    if (player->GetRoomId() != 0)
    {
        return false;
    }

    if (!mRooms.JoinRoom(roomId, playerId))
    {
        return false;
    }

    player->SetRoomId(roomId);
    player->SetReady(false);
    player->SetPlayAgain(false);
    return true;
}

bool HostEngine::RemovePlayerFromRoom(int playerId)
{
    ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return false;
    }

    bool removed = mRooms.RemovePlayerFromRoom(playerId);
    if (removed)
    {
        player->ClearRoomId();
        player->SetReady(false);
        player->SetPlayAgain(false);
    }

    return removed;
}

bool HostEngine::SetPlayerReady(int playerId, bool ready)
{
    ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return false;
    }

    if (player->GetRoomId() == 0)
    {
        return false;
    }

    player->SetReady(ready);
    return true;
}

bool HostEngine::AreAllPlayersReadyInRoom(int roomId) const
{
    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    if (static_cast<int>(room->playerIds.size()) < room->maxPlayers)
    {
        return false;
    }

    for (int playerId : room->playerIds)
    {
        const ConnectionProfile* profile = FindProfileInternal(playerId);
        if (profile == nullptr)
        {
            return false;
        }

        if (!profile->IsConnected() || !profile->IsReady())
        {
            return false;
        }
    }

    return true;
}

bool HostEngine::TryStartMatch(int roomId)
{
    if (!AreAllPlayersReadyInRoom(roomId))
    {
        return false;
    }

    return mRooms.StartRoomMatch(roomId);
}

bool HostEngine::SetPlayerPlayAgain(int playerId, bool playAgain)
{
    ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return false;
    }

    if (player->GetRoomId() == 0)
    {
        return false;
    }

    player->SetPlayAgain(playAgain);
    return true;
}

bool HostEngine::AreAllPlayersReadyForReplay(int roomId) const
{
    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    if (room->playerIds.empty())
    {
        return false;
    }

    for (int playerId : room->playerIds)
    {
        const ConnectionProfile* profile = FindProfileInternal(playerId);
        if (profile == nullptr)
        {
            return false;
        }

        if (!profile->IsConnected() || !profile->WantsPlayAgain())
        {
            return false;
        }
    }

    return true;
}

bool HostEngine::TryStartReplay(int roomId)
{
    if (!AreAllPlayersReadyForReplay(roomId))
    {
        return false;
    }

    RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    room->match.StartNextRound();
    ResetReplayVotes(roomId);
    return true;
}

void HostEngine::ResetReplayVotes(int roomId)
{
    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
    if (room == nullptr)
    {
        return;
    }

    for (int playerId : room->playerIds)
    {
        ConnectionProfile* profile = FindProfileInternal(playerId);
        if (profile != nullptr)
        {
            profile->SetPlayAgain(false);
            profile->SetReady(false);
        }
    }
}

CellResult HostEngine::SubmitCell(const CellSubmission& submission)
{
    const ConnectionProfile* player = FindProfileInternal(submission.playerId);
    if (player == nullptr)
    {
        return CellResult{};
    }

    if (player->GetRoomId() == 0)
    {
        return CellResult{};
    }

    RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return CellResult{};
    }

    return room->match.HandleCellSubmission(submission);
}

CellResult HostEngine::UseHint(int playerId, int row, int col)
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return CellResult{};
    }

    if (player->GetRoomId() == 0)
    {
        return CellResult{};
    }

    RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return CellResult{};
    }

    return room->match.UseHint(playerId, row, col);
}

std::vector<PlayerScoreInfo> HostEngine::GetLeaderboardForPlayer(int playerId) const
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return {};
    }

    if (player->GetRoomId() == 0)
    {
        return {};
    }

    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return {};
    }

    return room->match.GetLeaderboard();
}

HintState HostEngine::GetHintStateForPlayer(int playerId) const
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return HintState{};
    }

    if (player->GetRoomId() == 0)
    {
        return HintState{};
    }

    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return HintState{};
    }

    return room->match.GetHintStateForPlayer(playerId);
}

std::vector<MatchStatsSnapshot> HostEngine::GetMatchStatsForPlayer(int playerId) const
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return {};
    }

    if (player->GetRoomId() == 0)
    {
        return {};
    }

    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return {};
    }

    return room->match.GetMatchStats();
}

int HostEngine::GetElapsedMatchSecondsForPlayer(int playerId) const
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return 0;
    }

    if (player->GetRoomId() == 0)
    {
        return 0;
    }

    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return 0;
    }

    return room->match.GetElapsedMatchSeconds();
}

std::vector<PublicRoomInfo> HostEngine::GetPublicJoinableRooms() const
{
    std::vector<PublicRoomInfo> rooms;

    for (int roomId = 1; roomId <= 9999; ++roomId)
    {
        const RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
        if (room == nullptr)
        {
            continue;
        }

        if (room->visibility != RoomVisibility::Public)
        {
            continue;
        }

        int currentPlayers = static_cast<int>(room->playerIds.size());
        if (currentPlayers >= room->maxPlayers)
        {
            continue;
        }

        PublicRoomInfo info{};
        info.roomId = room->roomId;
        info.currentPlayers = currentPlayers;
        info.maxPlayers = room->maxPlayers;

        const ConnectionProfile* host = FindProfileInternal(room->hostPlayerId);
        if (host != nullptr)
        {
            info.hostName = host->GetPlayerName();
        }

        rooms.push_back(info);
    }

    return rooms;
}

const PuzzleData* HostEngine::GetPuzzleForPlayer(int playerId) const
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return nullptr;
    }

    if (player->GetRoomId() == 0)
    {
        return nullptr;
    }

    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(player->GetRoomId());
    if (room == nullptr)
    {
        return nullptr;
    }

    return &room->match.GetPuzzleData();
}

RoomSnapshot HostEngine::BuildRoomSnapshot(int roomId) const
{
    RoomSnapshot snapshot{};

    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
    if (room == nullptr)
    {
        return snapshot;
    }

    snapshot.config.maxPlayers = room->maxPlayers;
    snapshot.countdownStarted = false;

    for (int playerId : room->playerIds)
    {
        const ConnectionProfile* profile = FindProfileInternal(playerId);
        if (profile == nullptr)
        {
            continue;
        }

        PlayerScoreInfo info{};
        info.playerId = profile->GetPlayerId();
        info.playerName = profile->GetPlayerName();
        info.connected = profile->IsConnected();
        info.ready = profile->IsReady();
        info.finished = false;
        info.score = 0;
        info.rank = 0;
        info.hintUnlocked = false;
        info.currentStreak = 0;

        snapshot.players.push_back(info);
    }

    return snapshot;
}

RoomSnapshot HostEngine::BuildRoomSnapshotForPlayer(int playerId) const
{
    const ConnectionProfile* profile = FindProfileInternal(playerId);
    if (profile == nullptr)
    {
        return RoomSnapshot{};
    }

    if (profile->GetRoomId() == 0)
    {
        return RoomSnapshot{};
    }

    return BuildRoomSnapshot(profile->GetRoomId());
}

int HostEngine::GetPlayerRoomId(int playerId) const
{
    const ConnectionProfile* player = FindProfileInternal(playerId);
    if (player == nullptr)
    {
        return 0;
    }

    return player->GetRoomId();
}

ConnectionProfile* HostEngine::FindPlayerProfile(int playerId)
{
    return FindProfileInternal(playerId);
}

const ConnectionProfile* HostEngine::FindPlayerProfile(int playerId) const
{
    return FindProfileInternal(playerId);
}

ConnectionProfile* HostEngine::FindProfileInternal(int playerId)
{
    for (ConnectionProfile& player : mPlayers)
    {
        if (player.GetPlayerId() == playerId)
        {
            return &player;
        }
    }

    return nullptr;
}

const ConnectionProfile* HostEngine::FindProfileInternal(int playerId) const
{
    for (const ConnectionProfile& player : mPlayers)
    {
        if (player.GetPlayerId() == playerId)
        {
            return &player;
        }
    }

    return nullptr;
}

int HostEngine::GeneratePlayerId()
{
    return mNextPlayerId++;
}

bool HostEngine::IsHostOfRoom(int playerId, int roomId) const
{
    return mRooms.IsHost(roomId, playerId);
}

bool HostEngine::CloseRoom(int roomId)
{
    const RoomDirectory::RoomEntry* room = mRooms.FindRoom(roomId);
    if (room == nullptr)
    {
        return false;
    }

    for (int pid : room->playerIds)
    {
        ConnectionProfile* profile = FindProfileInternal(pid);
        if (profile != nullptr)
        {
            profile->ClearRoomId();
            profile->SetReady(false);
            profile->SetPlayAgain(false);
        }
    }

    return mRooms.CloseRoom(roomId);
}

std::vector<int> HostEngine::GetPlayersInRoom(int roomId) const
{
    return mRooms.GetPlayersInRoom(roomId);
}