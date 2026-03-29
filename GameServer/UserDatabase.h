#pragma once

#include <string>
#include <vector>
#include "../Shared/SharedModels.h"
#include "sqlite3.h"

class UserDatabase
{
public:
    UserDatabase();
    ~UserDatabase();

    bool Open(const std::string& dbPath);
    void Close();

    bool CreateUsersTable();
    bool CreateMatchHistoryTable();

    bool CreateFriendsTable();
    bool CreateFriendRequestsTable();

    bool UserExists(const std::string& username) const;
    bool RegisterUser(const std::string& username, const std::string& password);
    bool VerifyUser(const std::string& username, const std::string& password) const;

    bool UsernameExists(const std::string& username) const;

    bool FriendRequestExists(const std::string& fromUser, const std::string& toUser) const;
    bool AreFriends(const std::string& userA, const std::string& userB) const;

    bool InsertFriendRequest(const std::string& fromUser, const std::string& toUser);
    bool AcceptFriendRequest(const std::string& fromUser, const std::string& toUser);

    std::vector<FriendInfo> GetFriendsForUser(const std::string& username) const;
    std::vector<FriendInfo> GetPendingFriendRequestsForUser(const std::string& username) const;

    bool InsertMatchHistory(
        const std::string& username,
        const std::string& playedAt,
        const std::string& result,
        int finalScore,
        int wrongAttempts,
        int hintsUsed,
        int totalTimeSeconds,
        double averageMoveSeconds,
        int finalRank,
        int totalPlayers
    );

    std::vector<MatchHistoryRow> GetMatchHistoryForUser(const std::string& username) const;

private:
    std::string HashPassword(const std::string& password) const;

private:
    sqlite3* mDb = nullptr;
};