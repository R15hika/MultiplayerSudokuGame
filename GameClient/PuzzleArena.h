#pragma once

#include "PlayerViewport.h"
#include "ServerBridge.h"

class PuzzleArena
{
public:
    void SelectCell(PlayerViewport& viewport, int row, int col);

    void SubmitDigit(PlayerViewport& viewport, ServerBridge& bridge, int row, int col, int value);
    void ToggleNotes(PlayerViewport& viewport);
    void EraseCell(PlayerViewport& viewport, int row, int col);
    void UseHint(PlayerViewport& viewport, ServerBridge& bridge);

    void ProcessServerMessages(PlayerViewport& viewport, ServerBridge& bridge);
};