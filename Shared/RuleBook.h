#pragma once

namespace RuleBook
{
    constexpr int MIN_PLAYERS = 2;
    constexpr int MAX_PLAYERS = 3;

    constexpr int BOARD_SIZE = 9;
    constexpr int BOX_SIZE = 3;

    constexpr int FIRST_SOLVE_POINTS = 10;
    constexpr int SECOND_SOLVE_POINTS = 6;
    constexpr int THIRD_SOLVE_POINTS = 3;

    constexpr int WRONG_GUESS_PENALTY = 10;

    constexpr int HINT_UNLOCK_STREAK = 10;
    constexpr int HINT_UNLOCK_SCORE_LOW = 100;
    constexpr int HINT_UNLOCK_SCORE_HIGH = 150;
}