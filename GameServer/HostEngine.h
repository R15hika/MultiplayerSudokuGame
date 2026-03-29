#pragma once

#include <vector>

#include "../Shared/SharedModels.h"
#include "ConnectionProfile.h"
#include "RoomDirectory.h"

class HostEngine
{
public:
    HostEngine();

    int RegisterPlayer(const std::string& playerName);

    int CreateRoomForPlayer(int playerId, int maxPlayers, RoomVisibility visibility);
    bool JoinRoomForPlayer(int playerId, int roomId);
    bool RemovePlayerFromRoom(int playerId);

    bool SetPlayerReady(int playerId, bool ready);
    bool AreAllPlayersReadyInRoom(int roomId) const;
    bool TryStartMatch(int roomId);

    bool SetPlayerPlayAgain(int playerId, bool playAgain);
    bool AreAllPlayersReadyForReplay(int roomId) const;
    bool TryStartReplay(int roomId);
    void ResetReplayVotes(int roomId);

    CellResult SubmitCell(const CellSubmission& submission);
    CellResult UseHint(int playerId, int row, int col);

    std::vector<PlayerScoreInfo> GetLeaderboardForPlayer(int playerId) const;
    HintState GetHintStateForPlayer(int playerId) const;
    std::vector<MatchStatsSnapshot> GetMatchStatsForPlayer(int playerId) const;
    int GetElapsedMatchSecondsForPlayer(int playerId) const;
    const PuzzleData* GetPuzzleForPlayer(int playerId) const;

    std::vector<PublicRoomInfo> GetPublicJoinableRooms() const;

    RoomSnapshot BuildRoomSnapshot(int roomId) const;
    RoomSnapshot BuildRoomSnapshotForPlayer(int playerId) const;

    int GetPlayerRoomId(int playerId) const;
    ConnectionProfile* FindPlayerProfile(int playerId);
    const ConnectionProfile* FindPlayerProfile(int playerId) const;

    bool IsHostOfRoom(int playerId, int roomId) const;
    bool CloseRoom(int roomId);
    std::vector<int> GetPlayersInRoom(int roomId) const;

private:
    ConnectionProfile* FindProfileInternal(int playerId);
    const ConnectionProfile* FindProfileInternal(int playerId) const;
    int GeneratePlayerId();

private:
    std::vector<ConnectionProfile> mPlayers;
    RoomDirectory mRooms;
    int mNextPlayerId = 1;
};