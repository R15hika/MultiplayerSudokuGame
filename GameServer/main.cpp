#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include "HostEngine.h"
#include "SocketHub.h"
#include "UserDatabase.h"
#include "../Shared/WireFormat.h"

namespace
{

    std::vector<int> GetConnectionsForPlayers(
        const std::vector<int>& playerIds,
        const std::unordered_map<int, int>& connectionToPlayer)
    {
        std::vector<int> result;

        for (int pid : playerIds)
        {
            for (const auto& pair : connectionToPlayer)
            {
                if (pair.second == pid)
                {
                    result.push_back(pair.first);
                    break;
                }
            }
        }

        return result;
    }

    std::string GetCurrentDateTimeString()
    {
        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        #ifdef _WIN32
                localtime_s(&localTime, &now);
        #else
                localTime = *std::localtime(&now);
        #endif

        std::ostringstream oss;
        oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string GetUsernameForConnection(
        int connectionId,
        const std::unordered_map<int, int>& connectionToPlayer,
        const HostEngine& hostEngine)
    {
        auto mapIt = connectionToPlayer.find(connectionId);
        if (mapIt == connectionToPlayer.end())
        {
            return "";
        }

        const ConnectionProfile* profile = hostEngine.FindPlayerProfile(mapIt->second);
        return profile ? profile->GetPlayerName() : "";
    }

    int FindConnectionIdByUsername(
        const std::string& username,
        const std::unordered_map<int, int>& connectionToPlayer,
        const HostEngine& hostEngine)
    {
        for (const auto& pair : connectionToPlayer)
        {
            const ConnectionProfile* profile = hostEngine.FindPlayerProfile(pair.second);
            if (profile != nullptr && profile->GetPlayerName() == username)
            {
                return pair.first;
            }
        }
        return 0;
    }

    bool IsUsernameAlreadyLoggedIn(
        const std::string& username,
        const std::unordered_map<int, int>& connectionToPlayer,
        const HostEngine& hostEngine)
    {
        for (const auto& pair : connectionToPlayer)
        {
            const ConnectionProfile* profile = hostEngine.FindPlayerProfile(pair.second);
            if (profile != nullptr &&
                profile->GetPlayerName() == username &&
                profile->IsConnected())
            {
                return true;
            }
        }
        return false;
    }

    void SendToConnection(SocketHub& socketHub, int connectionId, const std::vector<std::uint8_t>& payload)
    {
        socketHub.SendPacket(connectionId, payload);
    }

    std::vector<int> GetConnectionsInRoom(
        int roomId,
        const HostEngine& hostEngine,
        const std::unordered_map<int, int>& connectionToPlayer)
    {
        std::vector<int> result;

        for (auto it = connectionToPlayer.begin(); it != connectionToPlayer.end(); ++it)
        {
            int connectionId = it->first;
            int playerId = it->second;

            if (hostEngine.GetPlayerRoomId(playerId) == roomId)
            {
                result.push_back(connectionId);
            }
        }

        return result;
    }

    void PatchLeaderboardNames(const HostEngine& hostEngine, std::vector<PlayerScoreInfo>& leaderboard)
    {
        for (std::size_t i = 0; i < leaderboard.size(); ++i)
        {
            const ConnectionProfile* profile = hostEngine.FindPlayerProfile(leaderboard[i].playerId);
            if (profile != nullptr)
            {
                leaderboard[i].playerName = profile->GetPlayerName();
                leaderboard[i].connected = profile->IsConnected();
            }
        }
    }

    void BroadcastToRoom(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, int>& connectionToPlayer,
        int roomId,
        const std::vector<std::uint8_t>& payload)
    {
        std::vector<int> connections = GetConnectionsInRoom(roomId, hostEngine, connectionToPlayer);

        for (std::size_t i = 0; i < connections.size(); ++i)
        {
            SendToConnection(socketHub, connections[i], payload);
        }
    }

    void BroadcastRoomSnapshot(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, int>& connectionToPlayer,
        int roomId)
    {
        RoomSnapshot snapshot = hostEngine.BuildRoomSnapshot(roomId);
        BroadcastToRoom(
            socketHub,
            hostEngine,
            connectionToPlayer,
            roomId,
            WireFormat::MakeWaitingRoomPacket(snapshot)
        );
    }

    void BroadcastMatchStarted(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, int>& connectionToPlayer,
        int roomId)
    {
        std::vector<int> connections = GetConnectionsInRoom(roomId, hostEngine, connectionToPlayer);

        for (std::size_t i = 0; i < connections.size(); ++i)
        {
            int connectionId = connections[i];
            auto mapIt = connectionToPlayer.find(connectionId);
            if (mapIt == connectionToPlayer.end())
            {
                continue;
            }

            int playerId = mapIt->second;
            const PuzzleData* puzzle = hostEngine.GetPuzzleForPlayer(playerId);
            if (puzzle != nullptr)
            {
                SendToConnection(socketHub, connectionId, WireFormat::MakeMatchStartedPacket(*puzzle));
            }
        }
    }

    void BroadcastLeaderboard(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, int>& connectionToPlayer,
        int roomId)
    {
        std::vector<int> connections = GetConnectionsInRoom(roomId, hostEngine, connectionToPlayer);
        if (connections.empty())
        {
            return;
        }

        auto mapIt = connectionToPlayer.find(connections[0]);
        if (mapIt == connectionToPlayer.end())
        {
            return;
        }

        std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(mapIt->second);
        PatchLeaderboardNames(hostEngine, leaderboard);

        BroadcastToRoom(
            socketHub,
            hostEngine,
            connectionToPlayer,
            roomId,
            WireFormat::MakeLeaderboardPacket(leaderboard)
        );
    }

    void SendError(SocketHub& socketHub, int connectionId, const std::string& text)
    {
        SendToConnection(socketHub, connectionId, WireFormat::MakeErrorPacket(text));
    }

    static void BroadcastPublicRoomList(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, bool>& connectionAuthenticated)
    {
        std::vector<PublicRoomInfo> rooms = hostEngine.GetPublicJoinableRooms();
        std::vector<std::uint8_t> payload = WireFormat::MakePublicRoomListResponsePacket(rooms);

        for (const auto& pair : connectionAuthenticated)
        {
            int connectionId = pair.first;
            bool authenticated = pair.second;

            if (authenticated)
            {
                SendToConnection(socketHub, connectionId, payload);
            }
        }
    }

    bool HasAnyFinished(const std::vector<PlayerScoreInfo>& leaderboard)
    {
        for (std::size_t i = 0; i < leaderboard.size(); ++i)
        {
            if (leaderboard[i].finished)
            {
                return true;
            }
        }

        return false;
    }

    std::string BuildRoundFinishedText(const std::vector<PlayerScoreInfo>& leaderboard)
    {
        if (leaderboard.empty())
        {
            return "Round finished.";
        }

        return "Round finished! Winner: " + leaderboard[0].playerName;
    }
}

int main()
{
    SocketHub socketHub;
    HostEngine hostEngine;
    UserDatabase userDb;
    std::unordered_map<int, int> connectionToPlayer;
    std::unordered_map<int, bool> connectionAuthenticated;

    if (!userDb.Open("sudoku_rush.db"))
    {
        std::cout << "Failed to open user database.\n";
        return 1;
    }

    if (!userDb.CreateUsersTable())
    {
        std::cout << "Failed to create users table.\n";
        return 1;
    }

    if (!userDb.CreateMatchHistoryTable())
    {
        std::cout << "Failed to create match history table.\n";
        return 1;
    }

    if (!userDb.CreateFriendsTable())
    {
        std::cout << "Failed to create friends table.\n";
        return 1;
    }

    if (!userDb.CreateFriendRequestsTable())
    {
        std::cout << "Failed to create friend requests table.\n";
        return 1;
    }

    if (!socketHub.Start(54000))
    {
        std::cout << "Failed to start server.\n";
        return 1;
    }

    std::cout << "Server started on port 54000.\n";

    bool running = true;

    while (running)
    {
        std::vector<SocketHub::Event> events;
        socketHub.PollEvents(events);

        for (std::size_t i = 0; i < events.size(); ++i)
        {
            const SocketHub::Event& event = events[i];

            if (event.type == SocketHub::EventType::Connected)
            {
                std::cout << "Client connected. ConnectionId = " << event.connectionId << "\n";
                continue;
            }

            if (event.type == SocketHub::EventType::Disconnected)
            {
                std::cout << "Client disconnected. ConnectionId = " << event.connectionId << "\n";

                auto mapIt = connectionToPlayer.find(event.connectionId);
                if (mapIt != connectionToPlayer.end())
                {
                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                    if (profile != nullptr)
                    {
                        profile->SetConnected(false);
                        profile->SetReady(false);
                        profile->SetPlayAgain(false);
                    }

                    bool wasHost = (roomId != 0) && hostEngine.IsHostOfRoom(playerId, roomId);

                    if (roomId != 0)
                    {
                        if (wasHost)
                        {
                            std::vector<int> playerIds = hostEngine.GetPlayersInRoom(roomId);
                            std::vector<int> roomConnections = GetConnectionsForPlayers(playerIds, connectionToPlayer);

                            hostEngine.CloseRoom(roomId);

                            for (int connId : roomConnections)
                            {
                                if (connId != event.connectionId)
                                {
                                    SendToConnection(
                                        socketHub,
                                        connId,
                                        WireFormat::MakeRoomClosedNoticePacket("Host disconnected. Room closed.")
                                    );
                                }
                            }
                        }
                        else
                        {
                            hostEngine.RemovePlayerFromRoom(playerId);
                        }
                    }

                    connectionToPlayer.erase(mapIt);
                    connectionAuthenticated.erase(event.connectionId);

                    if (roomId != 0 && !wasHost)
                    {
                        BroadcastRoomSnapshot(socketHub, hostEngine, connectionToPlayer, roomId);
                    }

                    if (roomId != 0)
                    {
                        BroadcastPublicRoomList(socketHub, hostEngine, connectionAuthenticated);
                    }
                }

                continue;
            }

            if (event.type != SocketHub::EventType::PacketReceived)
            {
                continue;
            }

            try
            {
                ByteReader reader(event.payload);
                PacketKind kind = WireFormat::ReadPacketKind(reader);

                switch (kind)
                {

                case PacketKind::SignUpRequest:
                {
                    AuthRequest request = WireFormat::ReadAuthRequest(reader);

                    AuthResponseData response{};

                    if (request.playerName.empty() || request.password.empty())
                    {
                        response.success = false;
                        response.text = "Username and password are required.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (userDb.UserExists(request.playerName))
                    {
                        response.success = false;
                        response.text = "Username already exists.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (!userDb.RegisterUser(request.playerName, request.password))
                    {
                        response.success = false;
                        response.text = "Failed to register user.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (IsUsernameAlreadyLoggedIn(request.playerName, connectionToPlayer, hostEngine))
                    {
                        response.success = false;
                        response.text = "This account is already logged in.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    int playerId = 0;
                    auto mapIt = connectionToPlayer.find(event.connectionId);

                    if (mapIt == connectionToPlayer.end())
                    {
                        playerId = hostEngine.RegisterPlayer(request.playerName);
                        connectionToPlayer[event.connectionId] = playerId;
                    }
                    else
                    {
                        playerId = mapIt->second;
                        ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                        if (profile != nullptr)
                        {
                            profile->SetPlayerName(request.playerName);
                        }
                    }

                    connectionAuthenticated[event.connectionId] = true;

                    response.success = true;
                    response.playerId = playerId;
                    response.text = "Sign up successful.";

                    SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                    break;
                }

                case PacketKind::SignInRequest:
                {
                    AuthRequest request = WireFormat::ReadAuthRequest(reader);

                    AuthResponseData response{};

                    if (request.playerName.empty() || request.password.empty())
                    {
                        response.success = false;
                        response.text = "Username and password are required.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (IsUsernameAlreadyLoggedIn(request.playerName, connectionToPlayer, hostEngine))
                    {
                        response.success = false;
                        response.text = "This account is already logged in.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (!userDb.UserExists(request.playerName))
                    {
                        response.success = false;
                        response.text = "User does not exist.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (!userDb.VerifyUser(request.playerName, request.password))
                    {
                        response.success = false;
                        response.text = "Wrong password.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    int playerId = 0;
                    auto mapIt = connectionToPlayer.find(event.connectionId);

                    if (mapIt == connectionToPlayer.end())
                    {
                        playerId = hostEngine.RegisterPlayer(request.playerName);
                        connectionToPlayer[event.connectionId] = playerId;
                    }
                    else
                    {
                        playerId = mapIt->second;
                        ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                        if (profile != nullptr)
                        {
                            profile->SetPlayerName(request.playerName);
                        }
                    }

                    connectionAuthenticated[event.connectionId] = true;

                    response.success = true;
                    response.playerId = playerId;
                    response.text = "Login successful.";

                    SendToConnection(socketHub, event.connectionId, WireFormat::MakeAuthResponsePacket(response));
                    break;
                }

                case PacketKind::CreateRoomRequest:
                {
                    CreateRoomRequest request = WireFormat::ReadCreateRoomRequest(reader);

                    auto authIt = connectionAuthenticated.find(event.connectionId);
                    if (authIt == connectionAuthenticated.end() || !authIt->second)
                    {
                        RoomJoinedResponseData response{};
                        response.success = false;
                        response.text = "Please log in first.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeRoomJoinedResponsePacket(response));
                        break;
                    }

                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.CreateRoomForPlayer(playerId, request.maxPlayers, request.visibility);

                    RoomJoinedResponseData response{};
                    response.success = (roomId != 0);
                    response.playerId = playerId;
                    response.roomId = roomId;
                    response.text = response.success ? "Room created." : "Failed to create room.";

                    SendToConnection(socketHub, event.connectionId, WireFormat::MakeRoomJoinedResponsePacket(response));

                    if (response.success)
                    {
                        BroadcastRoomSnapshot(socketHub, hostEngine, connectionToPlayer, roomId);
                        BroadcastPublicRoomList(socketHub, hostEngine, connectionAuthenticated);
                    }
                    break;
                }

                case PacketKind::JoinRoomRequest:
                {
                    JoinRoomRequest request = WireFormat::ReadJoinRoomRequest(reader);

                    auto authIt = connectionAuthenticated.find(event.connectionId);
                    if (authIt == connectionAuthenticated.end() || !authIt->second)
                    {
                        RoomJoinedResponseData response{};
                        response.success = false;
                        response.text = "Please log in first.";
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeRoomJoinedResponsePacket(response));
                        break;
                    }

                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    bool joined = hostEngine.JoinRoomForPlayer(playerId, request.roomId);

                    RoomJoinedResponseData response{};
                    response.success = joined;
                    response.playerId = playerId;
                    response.roomId = joined ? request.roomId : 0;
                    response.text = joined ? "Joined room." : "Failed to join room.";

                    SendToConnection(socketHub, event.connectionId, WireFormat::MakeRoomJoinedResponsePacket(response));

                    if (joined)
                    {
                        BroadcastRoomSnapshot(socketHub, hostEngine, connectionToPlayer, request.roomId);
                        BroadcastPublicRoomList(socketHub, hostEngine, connectionAuthenticated);
                    }
                    break;
                }

                case PacketKind::ReadyToggleRequest:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    bool ready = reader.ReadBool();

                    if (!hostEngine.SetPlayerReady(playerId, ready))
                    {
                        SendError(socketHub, event.connectionId, "Failed to update ready state.");
                        break;
                    }

                    int roomId = hostEngine.GetPlayerRoomId(playerId);
                    BroadcastRoomSnapshot(socketHub, hostEngine, connectionToPlayer, roomId);

                    if (hostEngine.TryStartMatch(roomId))
                    {
                        BroadcastMatchStarted(socketHub, hostEngine, connectionToPlayer, roomId);
                        BroadcastLeaderboard(socketHub, hostEngine, connectionToPlayer, roomId);
                    }
                    break;
                }

                case PacketKind::SubmitCellRequest:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    CellInputRequest request = WireFormat::ReadCellInputRequest(reader);

                    CellSubmission submission{};
                    submission.playerId = playerId;
                    submission.row = request.row;
                    submission.col = request.col;
                    submission.value = request.value;

                    CellResult result = hostEngine.SubmitCell(submission);

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeCellCheckedPacket(result)
                    );

                    if (result.correct && result.firstSolver != CellOwner::None)
                    {
                        CellClaimUpdate claim{};
                        claim.row = result.row;
                        claim.col = result.col;
                        claim.firstSolver = result.firstSolver;

                        BroadcastToRoom(
                            socketHub,
                            hostEngine,
                            connectionToPlayer,
                            roomId,
                            WireFormat::MakeCellClaimUpdatePacket(claim)
                        );
                    }

                    std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(playerId);
                    PatchLeaderboardNames(hostEngine, leaderboard);

                    BroadcastToRoom(
                        socketHub,
                        hostEngine,
                        connectionToPlayer,
                        roomId,
                        WireFormat::MakeLeaderboardPacket(leaderboard)
                    );

                    HintState hintState = hostEngine.GetHintStateForPlayer(playerId);
                    if (hintState.unlocked)
                    {
                        SendToConnection(socketHub, event.connectionId, WireFormat::MakeHintUnlockedPacket(hintState));
                    }

                    if (HasAnyFinished(leaderboard))
                    {
                        std::vector<MatchStatsSnapshot> stats = hostEngine.GetMatchStatsForPlayer(playerId);
                        int totalTimeSeconds = hostEngine.GetElapsedMatchSecondsForPlayer(playerId);
                        int totalPlayers = static_cast<int>(leaderboard.size());
                        std::string playedAt = GetCurrentDateTimeString();

                        for (const MatchStatsSnapshot& stat : stats)
                        {
                            const ConnectionProfile* profile = hostEngine.FindPlayerProfile(stat.playerId);
                            if (profile == nullptr)
                            {
                                continue;
                            }

                            std::string resultText = (stat.finalRank == 1) ? "WIN" : "LOSE";

                            double avgMoveSeconds = 0.0;
                            if (stat.moveCount > 0)
                            {
                                avgMoveSeconds = static_cast<double>(totalTimeSeconds) / static_cast<double>(stat.moveCount);
                            }

                            userDb.InsertMatchHistory(
                                profile->GetPlayerName(),
                                playedAt,
                                resultText,
                                stat.finalScore,
                                stat.wrongAttempts,
                                stat.hintsUsed,
                                totalTimeSeconds,
                                avgMoveSeconds,
                                stat.finalRank,
                                totalPlayers
                            );
                        }

                        BroadcastToRoom(
                            socketHub,
                            hostEngine,
                            connectionToPlayer,
                            roomId,
                            WireFormat::MakeRoundFinishedPacket(BuildRoundFinishedText(leaderboard))
                        );
                    }

                    break;
                }

                case PacketKind::TogglePlayAgainRequest:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);
                    bool playAgain = reader.ReadBool();

                    if (!hostEngine.SetPlayerPlayAgain(playerId, playAgain))
                    {
                        SendError(socketHub, event.connectionId, "Failed to update replay vote.");
                        break;
                    }

                    if (hostEngine.TryStartReplay(roomId))
                    {
                        BroadcastMatchStarted(socketHub, hostEngine, connectionToPlayer, roomId);
                        BroadcastLeaderboard(socketHub, hostEngine, connectionToPlayer, roomId);
                    }
                    break;
                }

                case PacketKind::UseHintRequest:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    HintRequest request = WireFormat::ReadHintRequest(reader);
                    CellResult hintResult = hostEngine.UseHint(playerId, request.row, request.col);

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeCellCheckedPacket(hintResult)
                    );

                    HintState hintState = hostEngine.GetHintStateForPlayer(playerId);
                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeHintUnlockedPacket(hintState)
                    );

                    std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(playerId);
                    PatchLeaderboardNames(hostEngine, leaderboard);

                    BroadcastToRoom(
                        socketHub,
                        hostEngine,
                        connectionToPlayer,
                        roomId,
                        WireFormat::MakeLeaderboardPacket(leaderboard)
                    );

                    if (HasAnyFinished(leaderboard))
                    {
                        BroadcastToRoom(
                            socketHub,
                            hostEngine,
                            connectionToPlayer,
                            roomId,
                            WireFormat::MakeRoundFinishedPacket(BuildRoundFinishedText(leaderboard))
                        );
                    }

                    break;
                }

                case PacketKind::SearchUserRequest:
                {
                    std::string targetUsername = reader.ReadString();

                    UserSearchResult result{};
                    result.username = targetUsername;

                    if (targetUsername.empty())
                    {
                        result.found = false;
                        result.text = "Username cannot be empty.";
                    }
                    else if (!userDb.UsernameExists(targetUsername))
                    {
                        result.found = false;
                        result.text = "User not found.";
                    }
                    else
                    {
                        result.found = true;
                        result.text = "User found.";
                    }

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeSearchUserResponsePacket(result)
                    );
                    break;
                }

                case PacketKind::SendFriendRequest:
                {
                    std::string fromUser = GetUsernameForConnection(event.connectionId, connectionToPlayer, hostEngine);
                    if (fromUser.empty())
                    {
                        SendError(socketHub, event.connectionId, "Not authenticated.");
                        break;
                    }

                    FriendRequestPayload payload = WireFormat::ReadFriendRequestPayload(reader);
                    const std::string& targetUser = payload.targetUsername;

                    std::cout << "[FriendRequest] from=" << fromUser
                        << " to=" << targetUser << "\n";

                    std::string responseText;

                    if (targetUser.empty())
                    {
                        responseText = "Target username cannot be empty.";
                    }
                    else if (targetUser == fromUser)
                    {
                        responseText = "Cannot add yourself.";
                    }
                    else if (!userDb.UsernameExists(targetUser))
                    {
                        responseText = "User not found.";
                    }
                    else if (userDb.AreFriends(fromUser, targetUser))
                    {
                        responseText = "You are already friends.";
                    }
                    else if (userDb.FriendRequestExists(fromUser, targetUser))
                    {
                        responseText = "Friend request already sent.";
                    }
                    else
                    {
                        if (userDb.InsertFriendRequest(fromUser, targetUser))
                        {
                            responseText = "Friend request sent.";

                            int targetConnectionId = FindConnectionIdByUsername(targetUser, connectionToPlayer, hostEngine);
                            if (targetConnectionId != 0)
                            {
                                FriendInfo info{};
                                info.username = fromUser;
                                std::cout << "[FriendRequestResult] " << responseText << "\n";
                                SendToConnection(
                                    socketHub,
                                    targetConnectionId,
                                    WireFormat::MakeFriendRequestReceivedPacket(info)
                                );
                            }
                        }
                        else
                        {
                            responseText = "Failed to send friend request.";
                        }
                    }

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeFriendRequestResponsePacket(responseText)
                    );
                    break;
                }

                case PacketKind::RespondFriendRequest:
                {
                    std::string currentUser = GetUsernameForConnection(event.connectionId, connectionToPlayer, hostEngine);
                    if (currentUser.empty())
                    {
                        SendError(socketHub, event.connectionId, "Not authenticated.");
                        break;
                    }

                    FriendRequestDecision decision = WireFormat::ReadFriendRequestDecision(reader);
                    std::string responseText;

                    if (decision.fromUsername.empty())
                    {
                        responseText = "Invalid request.";
                    }
                    else if (decision.accept)
                    {
                        if (userDb.AcceptFriendRequest(decision.fromUsername, currentUser))
                        {
                            responseText = "Friend request accepted.";
                        }
                        else
                        {
                            responseText = "Failed to accept friend request.";
                        }
                    }
                    else
                    {
                        responseText = "Friend request declined.";
                    }

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeFriendRequestResponsePacket(responseText)
                    );

                    std::vector<FriendInfo> updatedFriends = userDb.GetFriendsForUser(currentUser);
                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeFriendsListResponsePacket(updatedFriends)
                    );

                    std::vector<FriendInfo> updatedRequests = userDb.GetPendingFriendRequestsForUser(currentUser);
                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakePendingFriendRequestsResponsePacket(updatedRequests)
                    );

                    if (decision.accept)
                    {
                        int otherConnectionId = FindConnectionIdByUsername(
                            decision.fromUsername,
                            connectionToPlayer,
                            hostEngine
                        );

                        if (otherConnectionId != 0)
                        {
                            std::vector<FriendInfo> otherUpdatedFriends =
                                userDb.GetFriendsForUser(decision.fromUsername);

                            SendToConnection(
                                socketHub,
                                otherConnectionId,
                                WireFormat::MakeFriendsListResponsePacket(otherUpdatedFriends)
                            );

                            SendToConnection(
                                socketHub,
                                otherConnectionId,
                                WireFormat::MakeFriendRequestResponsePacket(
                                    currentUser + " accepted your friend request."
                                )
                            );
                        }
                    }

                    break;
                }

                case PacketKind::RequestFriendsList:
                {
                    std::string currentUser = GetUsernameForConnection(event.connectionId, connectionToPlayer, hostEngine);
                    if (currentUser.empty())
                    {
                        SendError(socketHub, event.connectionId, "Not authenticated.");
                        break;
                    }

                    std::vector<FriendInfo> friends = userDb.GetFriendsForUser(currentUser);
                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakeFriendsListResponsePacket(friends)
                    );
                    break;
                }

                case PacketKind::RequestPendingFriendRequests:
                {
                    std::string currentUser = GetUsernameForConnection(event.connectionId, connectionToPlayer, hostEngine);
                    if (currentUser.empty())
                    {
                        SendError(socketHub, event.connectionId, "Not authenticated.");
                        break;
                    }

                    std::vector<FriendInfo> requests = userDb.GetPendingFriendRequestsForUser(currentUser);
                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakePendingFriendRequestsResponsePacket(requests)
                    );
                    break;
                }

                case PacketKind::SendPrivateMessage:
                {
                    std::string fromUser = GetUsernameForConnection(event.connectionId, connectionToPlayer, hostEngine);
                    if (fromUser.empty())
                    {
                        SendError(socketHub, event.connectionId, "Not authenticated.");
                        break;
                    }

                    PrivateMessagePayload payload = WireFormat::ReadPrivateMessagePayload(reader);

                    if (payload.targetUsername.empty() || payload.text.empty())
                    {
                        SendError(socketHub, event.connectionId, "Invalid private message.");
                        break;
                    }

                    if (!userDb.AreFriends(fromUser, payload.targetUsername))
                    {
                        SendError(socketHub, event.connectionId, "You can only message friends.");
                        break;
                    }

                    PrivateChatMessage msg{};
                    msg.senderName = fromUser;
                    msg.receiverName = payload.targetUsername;
                    msg.text = payload.text;
                    msg.timestamp = GetCurrentDateTimeString();

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakePrivateMessageReceivedPacket(msg)
                    );

                    int targetConnectionId = FindConnectionIdByUsername(payload.targetUsername, connectionToPlayer, hostEngine);
                    if (targetConnectionId != 0)
                    {
                        SendToConnection(
                            socketHub,
                            targetConnectionId,
                            WireFormat::MakePrivateMessageReceivedPacket(msg)
                        );
                    }

                    break;
                }

                case PacketKind::HistoryRequest:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    const ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                    if (profile == nullptr)
                    {
                        SendError(socketHub, event.connectionId, "Player profile not found.");
                        break;
                    }

                    std::vector<MatchHistoryRow> history = userDb.GetMatchHistoryForUser(profile->GetPlayerName());
                    SendToConnection(socketHub, event.connectionId, WireFormat::MakeHistoryResponsePacket(history));
                    break;
                }

                case PacketKind::LeaveRoomRequest:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    if (roomId == 0)
                    {
                        LeaveRoomResponseData response{};
                        response.success = false;
                        response.text = "Not in a room.";
                        SendToConnection(socketHub, event.connectionId,
                            WireFormat::MakeLeaveRoomResponsePacket(response));
                        break;
                    }

                    bool isHost = hostEngine.IsHostOfRoom(playerId, roomId);

                    if (isHost)
                    {
                        std::vector<int> playerIds = hostEngine.GetPlayersInRoom(roomId);
                        std::vector<int> roomConnections = GetConnectionsForPlayers(playerIds, connectionToPlayer);

                        hostEngine.CloseRoom(roomId);

                        for (int connId : roomConnections)
                        {
                            SendToConnection(socketHub, connId,
                                WireFormat::MakeRoomClosedNoticePacket("Host left. Room closed."));
                        }

                        BroadcastPublicRoomList(socketHub, hostEngine, connectionAuthenticated);
                    }
                    else
                    {
                        bool removed = hostEngine.RemovePlayerFromRoom(playerId);

                        LeaveRoomResponseData response{};
                        response.success = removed;
                        response.text = removed ? "Left room." : "Failed to leave room.";

                        SendToConnection(socketHub, event.connectionId,
                            WireFormat::MakeLeaveRoomResponsePacket(response));

                        if (removed)
                        {
                            BroadcastRoomSnapshot(socketHub, hostEngine, connectionToPlayer, roomId);
                            BroadcastPublicRoomList(socketHub, hostEngine, connectionAuthenticated);
                        }
                    }

                    break;
                }

                case PacketKind::SendRoomChatMessage:
                {
                    auto mapIt = connectionToPlayer.find(event.connectionId);
                    if (mapIt == connectionToPlayer.end())
                    {
                        SendError(socketHub, event.connectionId, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    if (roomId == 0)
                    {
                        SendError(socketHub, event.connectionId, "Not in a room.");
                        break;
                    }

                    const ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                    if (profile == nullptr)
                    {
                        SendError(socketHub, event.connectionId, "Player profile not found.");
                        break;
                    }

                    ChatMessage msg{};
                    msg.senderName = profile->GetPlayerName();
                    msg.text = reader.ReadString();
                    msg.timestamp = GetCurrentDateTimeString();

                    BroadcastToRoom(
                        socketHub,
                        hostEngine,
                        connectionToPlayer,
                        roomId,
                        WireFormat::MakeRoomChatMessagePacket(msg)
                    );
                    break;
                }

                case PacketKind::RequestPublicRoomList:
                {
                    std::vector<PublicRoomInfo> rooms = hostEngine.GetPublicJoinableRooms();

                    SendToConnection(
                        socketHub,
                        event.connectionId,
                        WireFormat::MakePublicRoomListResponsePacket(rooms)
                    );
                    break;
                }

                default:
                    SendError(socketHub, event.connectionId, "Unhandled packet kind.");
                    break;
                }
            }
            catch (const std::exception& ex)
            {
                SendError(socketHub, event.connectionId, std::string("Packet decode error: ") + ex.what());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    socketHub.Stop();
    return 0;
}