#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "RuleBook.h"

enum class ScreenPhase : std::uint8_t
{
    StartMenu = 0,
    WaitingRoom,
    Countdown,
    Playing,
    RoundSummary,
    History
};

enum class CellOwner : std::uint8_t
{
    None = 0,
    Player1,
    Player2,
    Player3
};

enum class RoomVisibility : std::uint8_t
{
    Private = 0,
    Public
};
enum class EntryState : std::uint8_t
{
    Empty = 0,
    FixedClue,
    CorrectConfirmed,
    WrongTemporary,
    NoteOnly
};

struct CellPos
{
    int row = 0;
    int col = 0;

    bool IsValid() const
    {
        return row >= 0 && row < RuleBook::BOARD_SIZE &&
            col >= 0 && col < RuleBook::BOARD_SIZE;
    }
};

struct CellView
{
    int value = 0;
    EntryState state = EntryState::Empty;
    CellOwner firstSolver = CellOwner::None;
    bool selected = false;
};

struct PersonalNoteSet
{
    std::array<bool, 10> marked{};
};

struct PlayerScoreInfo
{
    int playerId = 0;
    std::string playerName;
    int score = 0;
    int rank = 0;
    bool connected = false;
    bool ready = false;
    bool finished = false;
    bool hintUnlocked = false;
    int currentStreak = 0;
    CellOwner displayColor = CellOwner::None;
};

struct RoomConfig
{
    int maxPlayers = RuleBook::MIN_PLAYERS;
};

struct RoomSnapshot
{
    RoomConfig config;
    std::vector<PlayerScoreInfo> players;
    bool countdownStarted = false;
};



struct PublicRoomInfo
{
    int roomId = 0;
    int currentPlayers = 0;
    int maxPlayers = RuleBook::MIN_PLAYERS;
    std::string hostName;
};

struct ChatMessage
{
    std::string senderName;
    std::string text;
    std::string timestamp;
};

struct LocalBoardState
{
    std::array<std::array<CellView, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE> cells{};
    std::array<std::array<PersonalNoteSet, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE> notes{};

    void ClearSelection()
    {
        for (auto& row : cells)
        {
            for (auto& cell : row)
            {
                cell.selected = false;
            }
        }
    }

    void SelectCell(int row, int col)
    {
        ClearSelection();

        if (row >= 0 && row < RuleBook::BOARD_SIZE &&
            col >= 0 && col < RuleBook::BOARD_SIZE)
        {
            cells[row][col].selected = true;
        }
    }
};

struct PuzzleData
{
    std::array<std::array<int, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE> puzzle{};
    std::array<std::array<int, RuleBook::BOARD_SIZE>, RuleBook::BOARD_SIZE> solution{};
};

struct CellSubmission
{
    int playerId = 0;
    int row = 0;
    int col = 0;
    int value = 0;
};

struct CellInputRequest
{
    int row = 0;
    int col = 0;
    int value = 0;
};

struct CellResult
{
    int playerId = 0;
    int row = 0;
    int col = 0;
    int submittedValue = 0;

    bool correct = false;
    bool awarded = false;
    int pointsAwarded = 0;

    CellOwner firstSolver = CellOwner::None;
};

struct CellClaimUpdate
{
    int row = 0;
    int col = 0;
    CellOwner firstSolver = CellOwner::None;
};

struct HintState
{
    bool unlocked = false;
    int availableCount = 0;
};

struct HintRequest
{
    int row = -1;
    int col = -1;
};

struct MatchSnapshot
{
    LocalBoardState board;
    std::vector<PlayerScoreInfo> leaderboard;
    HintState localHint;
    bool notesModeEnabled = false;
    bool roundFinished = false;
};

struct CreateRoomRequest
{
    int maxPlayers = RuleBook::MIN_PLAYERS;
    RoomVisibility visibility = RoomVisibility::Private;
};

struct JoinRoomRequest
{
    int roomId = 0;
};

struct AuthRequest
{
    std::string playerName;
    std::string password;
};

struct AuthResponseData
{
    bool success = false;
    int playerId = 0;
    std::string text;
};

struct RoomJoinedResponseData
{
    bool success = false;
    int playerId = 0;
    int roomId = 0;
    std::string text;
};

struct LeaveRoomResponseData
{
    bool success = false;
    std::string text;
};


struct MatchHistoryRow
{
    std::string playedAt;
    std::string result;
    int finalScore = 0;
    int wrongAttempts = 0;
    int hintsUsed = 0;
    int totalTimeSeconds = 0;
    double averageMoveSeconds = 0.0;
    int finalRank = 0;
    int totalPlayers = 0;
};

struct MatchStatsSnapshot
{
    int playerId = 0;
    int finalScore = 0;
    int wrongAttempts = 0;
    int hintsUsed = 0;
    int moveCount = 0;
    int finalRank = 0;
    bool finished = false;
};

struct UserSearchResult
{
    bool found = false;
    std::string username;
    std::string text;
};

struct FriendRequestPayload
{
    std::string targetUsername;
};

struct FriendRequestDecision
{
    std::string fromUsername;
    bool accept = false;
};

struct FriendInfo
{
    std::string username;
};

struct PrivateMessagePayload
{
    std::string targetUsername;
    std::string text;
};

struct PrivateChatMessage
{
    std::string senderName;
    std::string receiverName;
    std::string text;
    std::string timestamp;
};