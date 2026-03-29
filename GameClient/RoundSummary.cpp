#include "RoundSummary.h"

void RoundSummary::SetResultText(const std::string& text)
{
    mResultText = text;
}

const std::string& RoundSummary::GetResultText() const
{
    return mResultText;
}

void RoundSummary::UpdateFinalLeaderboard(const std::vector<PlayerScoreInfo>& leaderboard)
{
    mFinalLeaderboard = leaderboard;
}

const std::vector<PlayerScoreInfo>& RoundSummary::GetFinalLeaderboard() const
{
    return mFinalLeaderboard;
}

void RoundSummary::TogglePlayAgain(ServerBridge& bridge)
{
    if (!bridge.IsConnected())
    {
        return;
    }

    mPlayAgain = !mPlayAgain;
    bridge.SendPlayAgainRequest(mPlayAgain);
}

bool RoundSummary::WantsPlayAgain() const
{
    return mPlayAgain;
}

void RoundSummary::ProcessServerMessages(PlayerViewport& viewport, ServerBridge& bridge)
{
    while (bridge.HasPendingMessage())
    {
        BridgeMessage msg = bridge.PopNextMessage();

        switch (msg.kind)
        {
        case PacketKind::LeaderboardUpdate:
            UpdateFinalLeaderboard(msg.leaderboard);
            break;

        case PacketKind::RoundFinishedNotice:
            SetResultText(msg.text);
            viewport.SetPhase(ScreenPhase::RoundSummary);
            break;

        case PacketKind::MatchStarted:
            viewport.ResetForNewMatch();
            viewport.ApplyPuzzle(msg.puzzleData);
            viewport.SetPhase(ScreenPhase::Playing);
            mPlayAgain = false;
            mResultText.clear();
            mFinalLeaderboard.clear();
            break;

        default:
            break;
        }
    }
}