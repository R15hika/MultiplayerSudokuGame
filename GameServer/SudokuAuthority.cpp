#include "SudokuAuthority.h"

#include <algorithm>
#include <string>

SudokuAuthority::SudokuAuthority()
{
}

void SudokuAuthority::StartNewMatch(const PuzzleData& puzzleData, const std::vector<int>& playerIds)
{
    mPuzzleData = puzzleData;
    mPlayers.clear();
    mRoundFinished = false;
    mMatchStartTime = std::chrono::steady_clock::now();

    for (auto& row : mCellSolveOrder)
    {
        for (auto& cell : row)
        {
            cell.clear();
        }
    }

    for (int i = 0; i < static_cast<int>(playerIds.size()); ++i)
    {
        ServerPlayerState player{};
        player.info.playerId = playerIds[i];
        player.info.playerName.clear();
        player.info.score = 0;
        player.info.rank = 0;
        player.info.connected = true;
        player.info.ready = true;
        player.info.finished = false;
        player.info.hintUnlocked = false;
        player.info.currentStreak = 0;
        player.info.displayColor = ToDisplayColorByPlayerIndex(i);

        player.hint.unlocked = false;
        player.hint.availableCount = 0;
        player.correctStreak = 0;
        player.scoreHintGranted = false;
        player.streakHintsGranted = 0;
        player.wrongAttempts = 0;
        player.hintsUsed = 0;
        player.moveCount = 0;

        mPlayers.push_back(player);
    }

    RebuildLeaderboard();
}

CellResult SudokuAuthority::SubmitCell(const CellSubmission& submission)
{
    CellResult result{};
    result.playerId = submission.playerId;
    result.row = submission.row;
    result.col = submission.col;
    result.submittedValue = submission.value;
    result.correct = false;
    result.awarded = false;
    result.pointsAwarded = 0;
    result.firstSolver = CellOwner::None;

    if (mRoundFinished)
    {
        return result;
    }

    if (submission.row < 0 || submission.row >= RuleBook::BOARD_SIZE ||
        submission.col < 0 || submission.col >= RuleBook::BOARD_SIZE ||
        submission.value < 1 || submission.value > 9)
    {
        return result;
    }

    int playerIndex = FindPlayerIndex(submission.playerId);
    if (playerIndex < 0)
    {
        return result;
    }

    if (mPuzzleData.puzzle[submission.row][submission.col] != 0)
    {
        return result;
    }

    ServerPlayerState& player = mPlayers[playerIndex];
    player.moveCount += 1;

    bool alreadySolvedByThisPlayer = player.solvedCells[submission.row][submission.col];

    int correctValue = mPuzzleData.solution[submission.row][submission.col];
    if (submission.value != correctValue)
    {
        player.wrongAttempts += 1;
        player.correctStreak = 0;
        player.info.currentStreak = 0;
        result.correct = false;
        result.awarded = false;
        result.pointsAwarded = -RuleBook::WRONG_GUESS_PENALTY;

        player.info.score -= RuleBook::WRONG_GUESS_PENALTY;
        if (player.info.score < 0)
        {
            player.info.score = 0;
        }

        if (!mCellSolveOrder[submission.row][submission.col].empty())
        {
            int firstPlayerId = mCellSolveOrder[submission.row][submission.col][0];
            int firstIndex = FindPlayerIndex(firstPlayerId);

            if (firstIndex >= 0)
            {
                result.firstSolver = mPlayers[firstIndex].info.displayColor;
            }
        }

        RebuildLeaderboard();
        return result;
    }

    result.correct = true;

    if (!alreadySolvedByThisPlayer)
    {
        int solveRank = GetSolveRankForCell(submission.row, submission.col) + 1;

        if (solveRank <= RuleBook::MAX_PLAYERS)
        {
            mCellSolveOrder[submission.row][submission.col].push_back(submission.playerId);
        }

        player.solvedCells[submission.row][submission.col] = true;
        player.correctStreak += 1;
        player.info.currentStreak = player.correctStreak;

        result.awarded = true;
        result.pointsAwarded = PointsForSolveRank(solveRank);
        player.info.score += result.pointsAwarded;

        TryUnlockHint(player);

        if (CheckPlayerCompletedBoard(playerIndex))
        {
            player.info.finished = true;
            mRoundFinished = true;
        }
    }
    else
    {
        result.awarded = false;
        result.pointsAwarded = 0;
    }

    if (!mCellSolveOrder[submission.row][submission.col].empty())
    {
        int firstPlayerId = mCellSolveOrder[submission.row][submission.col][0];
        int firstIndex = FindPlayerIndex(firstPlayerId);

        if (firstIndex >= 0)
        {
            result.firstSolver = mPlayers[firstIndex].info.displayColor;
        }
    }

    RebuildLeaderboard();
    return result;
}

const PuzzleData& SudokuAuthority::GetPuzzleData() const
{
    return mPuzzleData;
}

std::vector<PlayerScoreInfo> SudokuAuthority::GetLeaderboard() const
{
    std::vector<PlayerScoreInfo> leaderboardCopy;

    for (const ServerPlayerState& player : mPlayers)
    {
        leaderboardCopy.push_back(player.info);
    }

    return leaderboardCopy;
}

bool SudokuAuthority::IsRoundFinished() const
{
    return mRoundFinished;
}

bool SudokuAuthority::HasPlayerFinished(int playerId) const
{
    int index = FindPlayerIndex(playerId);
    if (index < 0)
    {
        return false;
    }

    return mPlayers[index].info.finished;
}

HintState SudokuAuthority::GetHintStateForPlayer(int playerId) const
{
    int index = FindPlayerIndex(playerId);
    if (index < 0)
    {
        return HintState{};
    }

    return mPlayers[index].hint;
}

CellResult SudokuAuthority::UseHint(int playerId, int row, int col)
{
    CellResult result{};
    result.playerId = playerId;
    result.row = row;
    result.col = col;
    result.correct = false;
    result.awarded = false;
    result.pointsAwarded = 0;
    result.firstSolver = CellOwner::None;

    if (mRoundFinished)
    {
        return result;
    }

    if (row < 0 || row >= RuleBook::BOARD_SIZE ||
        col < 0 || col >= RuleBook::BOARD_SIZE)
    {
        return result;
    }

    int playerIndex = FindPlayerIndex(playerId);
    if (playerIndex < 0)
    {
        return result;
    }

    ServerPlayerState& player = mPlayers[playerIndex];

    if (!player.hint.unlocked || player.hint.availableCount <= 0)
    {
        return result;
    }

    if (mPuzzleData.puzzle[row][col] != 0)
    {
        return result;
    }

    if (player.solvedCells[row][col])
    {
        return result;
    }

    player.hint.availableCount -= 1;
    player.hintsUsed += 1;

    if (player.hint.availableCount < 0)
    {
        player.hint.availableCount = 0;
    }

    player.hint.unlocked = (player.hint.availableCount > 0);
    player.info.hintUnlocked = player.hint.unlocked;

    result.submittedValue = mPuzzleData.solution[row][col];
    result.correct = true;
    result.awarded = false;
    result.firstSolver = CellOwner::None;

    player.solvedCells[row][col] = true;
    player.correctStreak = 0;
    player.info.currentStreak = 0;

    if (CheckPlayerCompletedBoard(playerIndex))
    {
        player.info.finished = true;
        mRoundFinished = true;
    }

    RebuildLeaderboard();
    return result;
}

int SudokuAuthority::FindPlayerIndex(int playerId) const
{
    for (int i = 0; i < static_cast<int>(mPlayers.size()); ++i)
    {
        if (mPlayers[i].info.playerId == playerId)
        {
            return i;
        }
    }

    return -1;
}

int SudokuAuthority::GetSolveRankForCell(int row, int col) const
{
    return static_cast<int>(mCellSolveOrder[row][col].size());
}

CellOwner SudokuAuthority::ToDisplayColorByPlayerIndex(int playerIndex) const
{
    switch (playerIndex)
    {
    case 0: return CellOwner::Player1;
    case 1: return CellOwner::Player2;
    case 2: return CellOwner::Player3;
    default: return CellOwner::None;
    }
}

int SudokuAuthority::PointsForSolveRank(int solveRank) const
{
    switch (solveRank)
    {
    case 1: return RuleBook::FIRST_SOLVE_POINTS;
    case 2: return RuleBook::SECOND_SOLVE_POINTS;
    case 3: return RuleBook::THIRD_SOLVE_POINTS;
    default: return 0;
    }
}

void SudokuAuthority::RebuildLeaderboard()
{
    std::sort(mPlayers.begin(), mPlayers.end(),
        [](const ServerPlayerState& a, const ServerPlayerState& b)
        {
            if (a.info.score != b.info.score)
            {
                return a.info.score > b.info.score;
            }

            return a.info.playerId < b.info.playerId;
        });

    for (int i = 0; i < static_cast<int>(mPlayers.size()); ++i)
    {
        mPlayers[i].info.rank = i + 1;
    }
}

bool SudokuAuthority::CheckPlayerCompletedBoard(int playerIndex) const
{
    for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
    {
        for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
        {
            if (mPuzzleData.puzzle[row][col] == 0 &&
                !mPlayers[playerIndex].solvedCells[row][col])
            {
                return false;
            }
        }
    }

    return true;
}

void SudokuAuthority::TryUnlockHint(ServerPlayerState& playerState)
{
    // score-based unlock: only once when reaching 100+
    if (!playerState.scoreHintGranted &&
        playerState.info.score >= RuleBook::HINT_UNLOCK_SCORE_LOW)
    {
        playerState.scoreHintGranted = true;
        playerState.hint.availableCount += 1;
    }

    // streak-based unlocks:
    // 10 streak = +1
    // 20 streak = +1
    // 30 streak = +1
    // max 3 streak hints total per round
    int streakMilestonesReached = playerState.correctStreak / 10;
    if (streakMilestonesReached > 3)
    {
        streakMilestonesReached = 3;
    }

    if (streakMilestonesReached > playerState.streakHintsGranted)
    {
        int newlyEarned = streakMilestonesReached - playerState.streakHintsGranted;
        playerState.streakHintsGranted = streakMilestonesReached;
        playerState.hint.availableCount += newlyEarned;
    }

    playerState.hint.unlocked = (playerState.hint.availableCount > 0);
    playerState.info.hintUnlocked = playerState.hint.unlocked;
}

std::vector<MatchStatsSnapshot> SudokuAuthority::GetMatchStats() const
{
    std::vector<MatchStatsSnapshot> stats;

    for (const ServerPlayerState& player : mPlayers)
    {
        MatchStatsSnapshot s{};
        s.playerId = player.info.playerId;
        s.finalScore = player.info.score;
        s.wrongAttempts = player.wrongAttempts;
        s.hintsUsed = player.hintsUsed;
        s.moveCount = player.moveCount;
        s.finalRank = player.info.rank;
        s.finished = player.info.finished;
        stats.push_back(s);
    }

    return stats;
}

int SudokuAuthority::GetElapsedMatchSeconds() const
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mMatchStartTime).count();
    return static_cast<int>(elapsed);
}