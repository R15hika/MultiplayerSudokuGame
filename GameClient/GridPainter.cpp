#include "GridPainter.h"

#include <utility>

PaintedBoard GridPainter::BuildBoardModel(const LocalBoardState& board, bool notesModeEnabled) const
{
    PaintedBoard painted{};
    painted.notesModeEnabled = notesModeEnabled;
    painted.cells.reserve(RuleBook::BOARD_SIZE * RuleBook::BOARD_SIZE);

    for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
    {
        for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
        {
            const CellView& cell = board.cells[row][col];
            const PersonalNoteSet& noteSet = board.notes[row][col];

            PaintedCell paintedCell{};
            paintedCell.row = row;
            paintedCell.col = col;
            paintedCell.isFixed = (cell.state == EntryState::FixedClue);
            paintedCell.isSelected = cell.selected;
            paintedCell.isWrongTemporary = (cell.state == EntryState::WrongTemporary);
            paintedCell.hasMainValue = (cell.value != 0);
            paintedCell.mainValue = cell.value;
            paintedCell.firstSolver = cell.firstSolver;
            paintedCell.state = cell.state;

            if (cell.value == 0)
            {
                paintedCell.notes = CollectNotes(noteSet);
            }

            painted.cells.push_back(std::move(paintedCell));
        }
    }

    return painted;
}

std::vector<int> GridPainter::CollectNotes(const PersonalNoteSet& noteSet) const
{
    std::vector<int> notes;

    for (int value = 1; value <= 9; ++value)
    {
        if (noteSet.marked[value])
        {
            notes.push_back(value);
        }
    }

    return notes;
}