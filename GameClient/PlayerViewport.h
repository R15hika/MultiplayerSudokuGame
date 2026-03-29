#pragma once

#include <vector>
#include "../Shared/SharedModels.h"

class PlayerViewport
{
public:
    void ResetForNewMatch();
    void ApplyPuzzle(const PuzzleData& puzzleData);
    void ApplyCellResult(const CellResult& result);
    void ApplyCellClaimUpdate(const CellClaimUpdate& update);
    void UpdateLeaderboard(const std::vector<PlayerScoreInfo>& latest);
    void SetHintState(const HintState& state);

    void SelectCell(int row, int col);
    void SetNotesMode(bool enabled);
    bool IsNotesModeEnabled() const;

    LocalBoardState& GetBoard();
    const LocalBoardState& GetBoard() const;

    std::vector<PlayerScoreInfo>& GetLeaderboard();
    const std::vector<PlayerScoreInfo>& GetLeaderboard() const;

    HintState GetHintState() const;
    ScreenPhase GetPhase() const;
    void SetPhase(ScreenPhase next);

private:
    MatchSnapshot mSnapshot;
    ScreenPhase mPhase = ScreenPhase::StartMenu;
};