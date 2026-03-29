#include "LiveMatch.h"

LiveMatch::LiveMatch()
{
}

void LiveMatch::SetPlayers(const std::vector<int>& playerIds)
{
    mPlayerIds = playerIds;
}

const std::vector<int>& LiveMatch::GetPlayers() const
{
    return mPlayerIds;
}

void LiveMatch::StartNextRound()
{
    PuzzleData nextPuzzle = mBoardVault.CreateNextPuzzle();
    mAuthority.StartNewMatch(nextPuzzle, mPlayerIds);
    mRoundRunning = true;
}

CellResult LiveMatch::HandleCellSubmission(const CellSubmission& submission)
{
    if (!mRoundRunning)
    {
        return CellResult{};
    }

    CellResult result = mAuthority.SubmitCell(submission);

    if (mAuthority.IsRoundFinished())
    {
        mRoundRunning = false;
    }

    return result;
}

CellResult LiveMatch::UseHint(int playerId, int row, int col)
{
    if (!mRoundRunning)
    {
        return CellResult{};
    }

    CellResult result = mAuthority.UseHint(playerId, row, col);

    if (mAuthority.IsRoundFinished())
    {
        mRoundRunning = false;
    }

    return result;
}

const PuzzleData& LiveMatch::GetPuzzleData() const
{
    return mAuthority.GetPuzzleData();
}

std::vector<PlayerScoreInfo> LiveMatch::GetLeaderboard() const
{
    return mAuthority.GetLeaderboard();
}

HintState LiveMatch::GetHintStateForPlayer(int playerId) const
{
    return mAuthority.GetHintStateForPlayer(playerId);
}

std::vector<MatchStatsSnapshot> LiveMatch::GetMatchStats() const
{
    return mAuthority.GetMatchStats();
}

int LiveMatch::GetElapsedMatchSeconds() const
{
    return mAuthority.GetElapsedMatchSeconds();
}

bool LiveMatch::IsRoundRunning() const
{
    return mRoundRunning;
}

bool LiveMatch::IsRoundFinished() const
{
    return !mRoundRunning;
}

