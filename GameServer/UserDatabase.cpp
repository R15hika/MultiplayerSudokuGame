#include "UserDatabase.h"

#include <functional>
#include <ctime>
#include <iomanip>
#include <sstream>

UserDatabase::UserDatabase()
{
}

UserDatabase::~UserDatabase()
{
    Close();
}

bool UserDatabase::Open(const std::string& dbPath)
{
    Close();
    return sqlite3_open(dbPath.c_str(), &mDb) == SQLITE_OK;
}

void UserDatabase::Close()
{
    if (mDb != nullptr)
    {
        sqlite3_close(mDb);
        mDb = nullptr;
    }
}

bool UserDatabase::CreateUsersTable()
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY NOT NULL,"
        "password_hash TEXT NOT NULL"
        ");";

    char* errorMessage = nullptr;
    int rc = sqlite3_exec(mDb, sql, nullptr, nullptr, &errorMessage);

    if (errorMessage != nullptr)
    {
        sqlite3_free(errorMessage);
    }

    return rc == SQLITE_OK;
}

bool UserDatabase::CreateMatchHistoryTable()
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS match_history ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT NOT NULL,"
        "played_at TEXT NOT NULL,"
        "result TEXT NOT NULL,"
        "final_score INTEGER NOT NULL,"
        "wrong_attempts INTEGER NOT NULL,"
        "hints_used INTEGER NOT NULL,"
        "total_time_seconds INTEGER NOT NULL,"
        "average_move_seconds REAL NOT NULL,"
        "final_rank INTEGER NOT NULL,"
        "total_players INTEGER NOT NULL,"
        "FOREIGN KEY(username) REFERENCES users(username)"
        ");";

    char* errorMessage = nullptr;
    int rc = sqlite3_exec(mDb, sql, nullptr, nullptr, &errorMessage);

    if (errorMessage != nullptr)
    {
        sqlite3_free(errorMessage);
    }

    return rc == SQLITE_OK;
}

bool UserDatabase::CreateFriendsTable()
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS friends ("
        "user_a TEXT NOT NULL,"
        "user_b TEXT NOT NULL,"
        "PRIMARY KEY(user_a, user_b)"
        ");";

    char* errorMessage = nullptr;
    int rc = sqlite3_exec(mDb, sql, nullptr, nullptr, &errorMessage);

    if (errorMessage != nullptr)
    {
        sqlite3_free(errorMessage);
    }

    return rc == SQLITE_OK;
}

bool UserDatabase::CreateFriendRequestsTable()
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS friend_requests ("
        "from_user TEXT NOT NULL,"
        "to_user TEXT NOT NULL,"
        "PRIMARY KEY(from_user, to_user)"
        ");";

    char* errorMessage = nullptr;
    int rc = sqlite3_exec(mDb, sql, nullptr, nullptr, &errorMessage);

    if (errorMessage != nullptr)
    {
        sqlite3_free(errorMessage);
    }

    return rc == SQLITE_OK;
}

std::string UserDatabase::HashPassword(const std::string& password) const
{
    return std::to_string(std::hash<std::string>{}(password));
}

bool UserDatabase::UserExists(const std::string& username) const
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql = "SELECT 1 FROM users WHERE username = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

bool UserDatabase::RegisterUser(const std::string& username, const std::string& password)
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql = "INSERT INTO users (username, password_hash) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    std::string hash = HashPassword(password);

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);

    sqlite3_finalize(stmt);
    return ok;
}

bool UserDatabase::VerifyUser(const std::string& username, const std::string& password) const
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql = "SELECT password_hash FROM users WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = false;
    int rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW)
    {
        const unsigned char* storedHashText = sqlite3_column_text(stmt, 0);
        if (storedHashText != nullptr)
        {
            std::string storedHash = reinterpret_cast<const char*>(storedHashText);
            std::string incomingHash = HashPassword(password);
            ok = (storedHash == incomingHash);
        }
    }

    sqlite3_finalize(stmt);
    return ok;
}

bool UserDatabase::UsernameExists(const std::string& username) const
{
    return UserExists(username);
}

bool UserDatabase::FriendRequestExists(const std::string& fromUser, const std::string& toUser) const
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "SELECT 1 FROM friend_requests WHERE from_user = ? AND to_user = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, fromUser.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, toUser.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool UserDatabase::AreFriends(const std::string& userA, const std::string& userB) const
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "SELECT 1 FROM friends "
        "WHERE (user_a = ? AND user_b = ?) OR (user_a = ? AND user_b = ?) "
        "LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, userA.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, userB.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, userB.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, userA.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

bool UserDatabase::InsertFriendRequest(const std::string& fromUser, const std::string& toUser)
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "INSERT INTO friend_requests (from_user, to_user) VALUES (?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, fromUser.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, toUser.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool UserDatabase::AcceptFriendRequest(const std::string& fromUser, const std::string& toUser)
{
    if (mDb == nullptr)
    {
        return false;
    }

    sqlite3_exec(mDb, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const std::string a = (fromUser < toUser) ? fromUser : toUser;
    const std::string b = (fromUser < toUser) ? toUser : fromUser;

    const char* insertSql = "INSERT OR IGNORE INTO friends (user_a, user_b) VALUES (?, ?);";
    sqlite3_stmt* insertStmt = nullptr;
    bool ok = false;

    if (sqlite3_prepare_v2(mDb, insertSql, -1, &insertStmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(insertStmt, 1, a.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(insertStmt, 2, b.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(insertStmt) == SQLITE_DONE);
    }
    sqlite3_finalize(insertStmt);

    const char* deleteSql = "DELETE FROM friend_requests WHERE from_user = ? AND to_user = ?;";
    sqlite3_stmt* deleteStmt = nullptr;
    if (ok && sqlite3_prepare_v2(mDb, deleteSql, -1, &deleteStmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(deleteStmt, 1, fromUser.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(deleteStmt, 2, toUser.c_str(), -1, SQLITE_TRANSIENT);
        ok = (sqlite3_step(deleteStmt) == SQLITE_DONE);
    }
    sqlite3_finalize(deleteStmt);

    sqlite3_exec(mDb, ok ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
    return ok;
}

std::vector<FriendInfo> UserDatabase::GetFriendsForUser(const std::string& username) const
{
    std::vector<FriendInfo> rows;

    if (mDb == nullptr)
    {
        return rows;
    }

    const char* sql =
        "SELECT CASE WHEN user_a = ? THEN user_b ELSE user_a END AS friend_name "
        "FROM friends WHERE user_a = ? OR user_b = ? ORDER BY friend_name ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return rows;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text)
        {
            FriendInfo item{};
            item.username = reinterpret_cast<const char*>(text);
            rows.push_back(item);
        }
    }

    sqlite3_finalize(stmt);
    return rows;
}

std::vector<FriendInfo> UserDatabase::GetPendingFriendRequestsForUser(const std::string& username) const
{
    std::vector<FriendInfo> rows;

    if (mDb == nullptr)
    {
        return rows;
    }

    const char* sql =
        "SELECT from_user FROM friend_requests WHERE to_user = ? ORDER BY from_user ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return rows;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text)
        {
            FriendInfo item{};
            item.username = reinterpret_cast<const char*>(text);
            rows.push_back(item);
        }
    }

    sqlite3_finalize(stmt);
    return rows;
}

bool UserDatabase::InsertMatchHistory(
    const std::string& username,
    const std::string& playedAt,
    const std::string& result,
    int finalScore,
    int wrongAttempts,
    int hintsUsed,
    int totalTimeSeconds,
    double averageMoveSeconds,
    int finalRank,
    int totalPlayers)
{
    if (mDb == nullptr)
    {
        return false;
    }

    const char* sql =
        "INSERT INTO match_history ("
        "username, played_at, result, final_score, wrong_attempts, hints_used, "
        "total_time_seconds, average_move_seconds, final_rank, total_players"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, playedAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, finalScore);
    sqlite3_bind_int(stmt, 5, wrongAttempts);
    sqlite3_bind_int(stmt, 6, hintsUsed);
    sqlite3_bind_int(stmt, 7, totalTimeSeconds);
    sqlite3_bind_double(stmt, 8, averageMoveSeconds);
    sqlite3_bind_int(stmt, 9, finalRank);
    sqlite3_bind_int(stmt, 10, totalPlayers);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<MatchHistoryRow> UserDatabase::GetMatchHistoryForUser(const std::string& username) const
{
    std::vector<MatchHistoryRow> rows;

    if (mDb == nullptr)
    {
        return rows;
    }

    const char* sql =
        "SELECT played_at, result, final_score, wrong_attempts, hints_used, "
        "total_time_seconds, average_move_seconds, final_rank, total_players "
        "FROM match_history "
        "WHERE username = ? "
        "ORDER BY id DESC;";

    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(mDb, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        return rows;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        MatchHistoryRow row{};

        const unsigned char* playedAt = sqlite3_column_text(stmt, 0);
        const unsigned char* result = sqlite3_column_text(stmt, 1);

        row.playedAt = playedAt ? reinterpret_cast<const char*>(playedAt) : "";
        row.result = result ? reinterpret_cast<const char*>(result) : "";
        row.finalScore = sqlite3_column_int(stmt, 2);
        row.wrongAttempts = sqlite3_column_int(stmt, 3);
        row.hintsUsed = sqlite3_column_int(stmt, 4);
        row.totalTimeSeconds = sqlite3_column_int(stmt, 5);
        row.averageMoveSeconds = sqlite3_column_double(stmt, 6);
        row.finalRank = sqlite3_column_int(stmt, 7);
        row.totalPlayers = sqlite3_column_int(stmt, 8);

        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}