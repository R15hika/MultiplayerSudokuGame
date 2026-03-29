#include "PlayerViewport.h"

static void RemoveCandidateFromPeers(LocalBoardState& board, int row, int col, int value)
{
    if (value < 1 || value > 9)
    {
        return;
    }

    // same row
    for (int c = 0; c < RuleBook::BOARD_SIZE; ++c)
    {
        if (c == col) continue;
        board.notes[row][c].marked[value] = false;
    }

    // same column
    for (int r = 0; r < RuleBook::BOARD_SIZE; ++r)
    {
        if (r == row) continue;
        board.notes[r][col].marked[value] = false;
    }

    // same 3x3 box
    int boxRowStart = (row / RuleBook::BOX_SIZE) * RuleBook::BOX_SIZE;
    int boxColStart = (col / RuleBook::BOX_SIZE) * RuleBook::BOX_SIZE;

    for (int r = boxRowStart; r < boxRowStart + RuleBook::BOX_SIZE; ++r)
    {
        for (int c = boxColStart; c < boxColStart + RuleBook::BOX_SIZE; ++c)
        {
            if (r == row && c == col) continue;
            board.notes[r][c].marked[value] = false;
        }
    }
}

void PlayerViewport::ResetForNewMatch()
{
    mSnapshot = MatchSnapshot{};
    mPhase = ScreenPhase::Countdown;
}

void PlayerViewport::ApplyPuzzle(const PuzzleData& puzzleData)
{
    for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
    {
        for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
        {
            int value = puzzleData.puzzle[row][col];
            CellView& cell = mSnapshot.board.cells[row][col];

            cell.value = value;
            cell.firstSolver = CellOwner::None;
            cell.selected = false;

            if (value != 0)
            {
                cell.state = EntryState::FixedClue;
            }
            else
            {
                cell.state = EntryState::Empty;
            }

            mSnapshot.board.notes[row][col] = PersonalNoteSet{};
        }
    }

    mPhase = ScreenPhase::Playing;
}

void PlayerViewport::ApplyCellResult(const CellResult& result)
{
    if (result.row < 0 || result.row >= RuleBook::BOARD_SIZE ||
        result.col < 0 || result.col >= RuleBook::BOARD_SIZE)
    {
        return;
    }

    CellView& cell = mSnapshot.board.cells[result.row][result.col];

    if (cell.state == EntryState::FixedClue)
    {
        return;
    }

    if (result.correct)
    {
        cell.value = result.submittedValue;
        cell.state = EntryState::CorrectConfirmed;
        cell.firstSolver = result.firstSolver;

        // clear notes in this cell itself
        mSnapshot.board.notes[result.row][result.col] = PersonalNoteSet{};

        // remove this value from notes in same row / col / box
        RemoveCandidateFromPeers(mSnapshot.board, result.row, result.col, result.submittedValue);
    }
    else
    {
        cell.value = result.submittedValue;
        cell.state = EntryState::WrongTemporary;
    }
}

void PlayerViewport::ApplyCellClaimUpdate(const CellClaimUpdate& update)
{
    if (update.row < 0 || update.row >= RuleBook::BOARD_SIZE ||
        update.col < 0 || update.col >= RuleBook::BOARD_SIZE)
    {
        return;
    }

    CellView& cell = mSnapshot.board.cells[update.row][update.col];
    cell.firstSolver = update.firstSolver;
}

void PlayerViewport::UpdateLeaderboard(const std::vector<PlayerScoreInfo>& latest)
{
    mSnapshot.leaderboard = latest;
}

void PlayerViewport::SetHintState(const HintState& state)
{
    mSnapshot.localHint = state;
}

void PlayerViewport::SelectCell(int row, int col)
{
    mSnapshot.board.SelectCell(row, col);
}

void PlayerViewport::SetNotesMode(bool enabled)
{
    mSnapshot.notesModeEnabled = enabled;
}

bool PlayerViewport::IsNotesModeEnabled() const
{
    return mSnapshot.notesModeEnabled;
}

LocalBoardState& PlayerViewport::GetBoard()
{
    return mSnapshot.board;
}

const LocalBoardState& PlayerViewport::GetBoard() const
{
    return mSnapshot.board;
}

std::vector<PlayerScoreInfo>& PlayerViewport::GetLeaderboard()
{
    return mSnapshot.leaderboard;
}

const std::vector<PlayerScoreInfo>& PlayerViewport::GetLeaderboard() const
{
    return mSnapshot.leaderboard;
}

HintState PlayerViewport::GetHintState() const
{
    return mSnapshot.localHint;
}

ScreenPhase PlayerViewport::GetPhase() const
{
    return mPhase;
}

void PlayerViewport::SetPhase(ScreenPhase next)
{
    mPhase = next;
}