#include "PuzzleArena.h"

void PuzzleArena::SelectCell(PlayerViewport& viewport, int row, int col)
{
    viewport.SelectCell(row, col);
}

void PuzzleArena::SubmitDigit(PlayerViewport& viewport, ServerBridge& bridge, int row, int col, int value)
{
    if (!bridge.IsConnected())
    {
        return;
    }

    if (row < 0 || row >= RuleBook::BOARD_SIZE ||
        col < 0 || col >= RuleBook::BOARD_SIZE)
    {
        return;
    }

    if (value < 1 || value > 9)
    {
        return;
    }

    LocalBoardState& board = viewport.GetBoard();
    CellView& cell = board.cells[row][col];

    if (cell.state == EntryState::FixedClue)
    {
        return;
    }

    if (viewport.IsNotesModeEnabled())
    {
        board.notes[row][col].marked[value] = !board.notes[row][col].marked[value];
        return;
    }

    bridge.SendSubmitCellRequest(row, col, value);
}

void PuzzleArena::ToggleNotes(PlayerViewport& viewport)
{
    viewport.SetNotesMode(!viewport.IsNotesModeEnabled());
}

void PuzzleArena::EraseCell(PlayerViewport& viewport, int row, int col)
{
    if (row < 0 || row >= RuleBook::BOARD_SIZE ||
        col < 0 || col >= RuleBook::BOARD_SIZE)
    {
        return;
    }

    LocalBoardState& board = viewport.GetBoard();
    CellView& cell = board.cells[row][col];

    if (cell.state == EntryState::FixedClue)
    {
        return;
    }

    if (viewport.IsNotesModeEnabled())
    {
        board.notes[row][col] = PersonalNoteSet{};
        return;
    }

    cell.value = 0;
    cell.state = EntryState::Empty;
}

void PuzzleArena::UseHint(PlayerViewport& viewport, ServerBridge& bridge)
{
    if (!bridge.IsConnected())
    {
        return;
    }

    int selectedRow = -1;
    int selectedCol = -1;

    const LocalBoardState& board = viewport.GetBoard();
    for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
    {
        for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
        {
            if (board.cells[row][col].selected)
            {
                selectedRow = row;
                selectedCol = col;
                break;
            }
        }
        if (selectedRow != -1)
        {
            break;
        }
    }

    if (selectedRow == -1 || selectedCol == -1)
    {
        return;
    }

    bridge.SendUseHintRequest(selectedRow, selectedCol);
}

void PuzzleArena::ProcessServerMessages(PlayerViewport& viewport, ServerBridge& bridge)
{
    while (bridge.HasPendingMessage())
    {
        BridgeMessage msg = bridge.PopNextMessage();

        switch (msg.kind)
        {
        case PacketKind::CellCheckedResponse:
            viewport.ApplyCellResult(msg.cellResult);
            break;

        case PacketKind::LeaderboardUpdate:
            viewport.UpdateLeaderboard(msg.leaderboard);
            break;

        case PacketKind::HintUnlockedNotice:
            viewport.SetHintState(msg.hintState);
            break;

        case PacketKind::RoundFinishedNotice:
            viewport.SetPhase(ScreenPhase::RoundSummary);
            break;

        default:
            break;
        }
    }
}