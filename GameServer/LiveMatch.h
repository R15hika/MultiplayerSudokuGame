#pragma once

#include <vector>

#include "../Shared/SharedModels.h"
#include "BoardVault.h"
#include "SudokuAuthority.h"

class LiveMatch
{
public:
    LiveMatch();

    void SetPlayers(const std::vector<int>& playerIds);
    const std::vector<int>& GetPlayers() const;

    void StartNextRound();

    CellResult HandleCellSubmission(const CellSubmission& submission);
    CellResult UseHint(int playerId, int row, int col);

    const PuzzleData& GetPuzzleData() const;
    std::vector<PlayerScoreInfo> GetLeaderboard() const;


    HintState GetHintStateForPlayer(int playerId) const;
    std::vector<MatchStatsSnapshot> GetMatchStats() const;
    int GetElapsedMatchSeconds() const;

    bool IsRoundRunning() const;
    bool IsRoundFinished() const;

private:
    std::vector<int> mPlayerIds;
    BoardVault mBoardVault;
    SudokuAuthority mAuthority;
    bool mRoundRunning = false;
};