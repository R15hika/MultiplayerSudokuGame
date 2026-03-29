#pragma once

#include <vector>

#include "../Shared/SharedModels.h"

struct PaintedCell
{
    int row = 0;
    int col = 0;

    bool isFixed = false;
    bool isSelected = false;
    bool isWrongTemporary = false;
    bool hasMainValue = false;

    int mainValue = 0;
    CellOwner firstSolver = CellOwner::None;
    EntryState state = EntryState::Empty;

    std::vector<int> notes;
};

struct PaintedBoard
{
    std::vector<PaintedCell> cells;
    bool notesModeEnabled = false;
};

class GridPainter
{
public:
    PaintedBoard BuildBoardModel(const LocalBoardState& board, bool notesModeEnabled) const;

private:
    std::vector<int> CollectNotes(const PersonalNoteSet& noteSet) const;
};