#pragma once

#include <array>
#include <vector>
#include <chrono>

#include "../Shared/SharedModels.h"
#include "../Shared/RuleBook.h"

class SudokuAuthority
{
public:
    SudokuAuthority();

    void StartNewMatch(const PuzzleData& puzzleData, const std::vector<int>& playerIds);

    CellResult SubmitCell(const CellSubmission& submission);

    const PuzzleData& GetPuzzleData() const;
    std::vector<PlayerScoreInfo> GetLeaderboard() const;

    bool IsRoundFinished() const;
    bool HasPlayerFinished(int playerId) const;

    HintState GetHintStateForPlayer(int playerId) const;

    CellResult UseHint(int playerId, int row, int col);

    std::vector<MatchStatsSnapshot> GetMatchStats() const;
    int GetElapsedMatchSeconds() const;


struct ServerPlayerState
{
    PlayerScoreInfo info;
    HintState hint;
    std::array<std::array<bool, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE> solvedCells{};
    int correctStreak = 0;

    bool scoreHintGranted = false; // only grant 100-score hint once per round
    int streakHintsGranted = 0; // total streak hints granted this round, max 3

    int wrongAttempts = 0;
    int hintsUsed = 0;
    int moveCount = 0;
};

private:
    int FindPlayerIndex(int playerId) const;
    int GetSolveRankForCell(int row, int col) const;
    CellOwner ToDisplayColorByPlayerIndex(int playerIndex) const;
    int PointsForSolveRank(int solveRank) const;
    void RebuildLeaderboard();
    bool CheckPlayerCompletedBoard(int playerIndex) const;
    void TryUnlockHint(ServerPlayerState& playerState);

private:
    PuzzleData mPuzzleData;
    std::vector<ServerPlayerState> mPlayers;
    std::array<std::array<std::vector<int>, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE> mCellSolveOrder{};
    bool mRoundFinished = false;
    std::chrono::steady_clock::time_point mMatchStartTime;
};