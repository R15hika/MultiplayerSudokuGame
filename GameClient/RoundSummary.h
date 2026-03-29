#pragma once

#include <string>
#include <vector>

#include "../Shared/SharedModels.h"
#include "PlayerViewport.h"
#include "ServerBridge.h"

class RoundSummary
{
public:
    void SetResultText(const std::string& text);
    const std::string& GetResultText() const;

    void UpdateFinalLeaderboard(const std::vector<PlayerScoreInfo>& leaderboard);
    const std::vector<PlayerScoreInfo>& GetFinalLeaderboard() const;

    void TogglePlayAgain(ServerBridge& bridge);
    bool WantsPlayAgain() const;

    void ProcessServerMessages(PlayerViewport& viewport, ServerBridge& bridge);

private:
    std::string mResultText;
    std::vector<PlayerScoreInfo> mFinalLeaderboard;
    bool mPlayAgain = false;
};