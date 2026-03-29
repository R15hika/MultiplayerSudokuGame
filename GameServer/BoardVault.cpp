#include "BoardVault.h"

#include <algorithm>
#include <random>
#include <vector>

BoardVault::BoardVault()
    : mRng(std::random_device{}())
{
    mCurrentPuzzle = GenerateRandomPuzzle();
}

const PuzzleData& BoardVault::GetCurrentPuzzle() const
{
    return mCurrentPuzzle;
}

PuzzleData BoardVault::CreateNextPuzzle()
{
    mCurrentPuzzle = GenerateRandomPuzzle();
    return mCurrentPuzzle;
}

PuzzleData BoardVault::GenerateRandomPuzzle()
{
    PuzzleData data{};
    Grid solved{};

    FillSolvedGrid(solved);
    data.solution = solved;
    data.puzzle = solved;

    std::vector<int> positions;
    positions.reserve(RuleBook::BOARD_SIZE * RuleBook::BOARD_SIZE);

    for (int i = 0; i < RuleBook::BOARD_SIZE * RuleBook::BOARD_SIZE; ++i)
    {
        positions.push_back(i);
    }

    std::shuffle(positions.begin(), positions.end(), mRng);

    int cluesToKeep = 32; // adjust later for difficulty
    int cellsToRemove = 81 - cluesToKeep;

    int removed = 0;
    for (int index : positions)
    {
        if (removed >= cellsToRemove)
        {
            break;
        }

        int row = index / RuleBook::BOARD_SIZE;
        int col = index % RuleBook::BOARD_SIZE;

        int backup = data.puzzle[row][col];
        data.puzzle[row][col] = 0;

        Grid testGrid = data.puzzle;
        int solutions = CountSolutions(testGrid, 2);

        if (solutions != 1)
        {
            data.puzzle[row][col] = backup;
        }
        else
        {
            ++removed;
        }
    }

    return data;
}

bool BoardVault::FillSolvedGrid(Grid& grid)
{
    return FillCell(grid, 0, 0);
}

bool BoardVault::FillCell(Grid& grid, int row, int col)
{
    if (row == RuleBook::BOARD_SIZE)
    {
        return true;
    }

    int nextRow = (col == RuleBook::BOARD_SIZE - 1) ? row + 1 : row;
    int nextCol = (col == RuleBook::BOARD_SIZE - 1) ? 0 : col + 1;

    if (grid[row][col] != 0)
    {
        return FillCell(grid, nextRow, nextCol);
    }

    std::vector<int> values{ 1,2,3,4,5,6,7,8,9 };
    std::shuffle(values.begin(), values.end(), mRng);

    for (int value : values)
    {
        if (IsSafe(grid, row, col, value))
        {
            grid[row][col] = value;

            if (FillCell(grid, nextRow, nextCol))
            {
                return true;
            }

            grid[row][col] = 0;
        }
    }

    return false;
}

bool BoardVault::IsSafe(const Grid& grid, int row, int col, int value) const
{
    for (int i = 0; i < RuleBook::BOARD_SIZE; ++i)
    {
        if (grid[row][i] == value)
        {
            return false;
        }

        if (grid[i][col] == value)
        {
            return false;
        }
    }

    int boxRowStart = (row / RuleBook::BOX_SIZE) * RuleBook::BOX_SIZE;
    int boxColStart = (col / RuleBook::BOX_SIZE) * RuleBook::BOX_SIZE;

    for (int r = 0; r < RuleBook::BOX_SIZE; ++r)
    {
        for (int c = 0; c < RuleBook::BOX_SIZE; ++c)
        {
            if (grid[boxRowStart + r][boxColStart + c] == value)
            {
                return false;
            }
        }
    }

    return true;
}

bool BoardVault::FindEmptyCell(const Grid& grid, int& outRow, int& outCol) const
{
    for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
    {
        for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
        {
            if (grid[row][col] == 0)
            {
                outRow = row;
                outCol = col;
                return true;
            }
        }
    }

    return false;
}

int BoardVault::CountSolutions(Grid& grid, int limit)
{
    int row = 0;
    int col = 0;

    if (!FindEmptyCell(grid, row, col))
    {
        return 1;
    }

    int total = 0;
    for (int value = 1; value <= 9; ++value)
    {
        if (IsSafe(grid, row, col, value))
        {
            grid[row][col] = value;
            total += CountSolutions(grid, limit);

            if (total >= limit)
            {
                grid[row][col] = 0;
                return total;
            }

            grid[row][col] = 0;
        }
    }

    return total;
}