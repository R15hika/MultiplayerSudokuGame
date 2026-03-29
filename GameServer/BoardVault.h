#pragma once

#include <array>
#include <vector>
#include <random>
#include "../Shared/SharedModels.h"

class BoardVault
{
public:
    BoardVault();

    const PuzzleData& GetCurrentPuzzle() const;
    PuzzleData CreateNextPuzzle();

private:
    using Grid = std::array<std::array<int, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE>;

    PuzzleData GenerateRandomPuzzle();
    bool FillSolvedGrid(Grid& grid);
    bool FillCell(Grid& grid, int row, int col);
    bool IsSafe(const Grid& grid, int row, int col, int value) const;

    int CountSolutions(Grid& grid, int limit);
    bool FindEmptyCell(const Grid& grid, int& outRow, int& outCol) const;

private:
    PuzzleData mCurrentPuzzle;
    std::mt19937 mRng;
};