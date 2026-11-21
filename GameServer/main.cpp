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
    std::uint32_t gServerSequence = 1;

    constexpr long long kWaitingRoomIdleMs = 85000;
    constexpr long long kPlayingWarnIdleMs = 55000;
    constexpr long long kPlayingTimeoutMs =  85000;

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

    std::string GetUsernameForEndpoint(
        const NetAddress& endpoint,
        const std::unordered_map<NetAddress, int, NetAddressHash>& endpointToPlayer,
        const HostEngine& hostEngine)
    {
        auto mapIt = endpointToPlayer.find(endpoint);
        if (mapIt == endpointToPlayer.end())
        {
            return "";
        }

        const ConnectionProfile* profile = hostEngine.FindPlayerProfile(mapIt->second);
        return profile ? profile->GetPlayerName() : "";
    }

    NetAddress* FindEndpointByUsername(
        const std::string& username,
        std::unordered_map<int, NetAddress>& playerToEndpoint,
        const HostEngine& hostEngine)
    {
        for (auto& pair : playerToEndpoint)
        {
            const ConnectionProfile* profile = hostEngine.FindPlayerProfile(pair.first);
            if (profile != nullptr && profile->GetPlayerName() == username && profile->IsConnected())
            {
                return &pair.second;
            }
        }
        return nullptr;
    }

    std::vector<NetAddress> GetEndpointsForPlayers(
        const std::vector<int>& playerIds,
        const std::unordered_map<int, NetAddress>& playerToEndpoint)
    {
        std::vector<NetAddress> result;

        for (int pid : playerIds)
        {
            auto it = playerToEndpoint.find(pid);
            if (it != playerToEndpoint.end())
            {
                result.push_back(it->second);
            }
        }

        return result;
    }

    void SendToEndpoint(
        SocketHub& socketHub,
        const NetAddress& endpoint,
        const std::vector<std::uint8_t>& payload)
    {
        socketHub.SendPacket(endpoint, payload);
    }

    std::vector<std::uint8_t> WrapUdpPayload(
        const std::vector<std::uint8_t>& payload,
        std::uint32_t sequence = 1,
        std::uint32_t ack = 0,
        bool isAckOnly = false)
    {
        ByteWriter writer;
        writer.WriteUInt32(sequence);
        writer.WriteUInt32(ack);
        writer.WriteBool(isAckOnly);

        for (std::uint8_t b : payload)
        {
            writer.WriteUInt8(b);
        }

        return writer.GetBuffer();
    }

    void SendWrapped(
        SocketHub& socketHub,
        const NetAddress& endpoint,
        const std::vector<std::uint8_t>& payload,
        std::uint32_t ack = 0)
    {
        SendToEndpoint(socketHub, endpoint, WrapUdpPayload(payload, gServerSequence++, ack, false));
    }

    void SendAckOnly(
        SocketHub& socketHub,
        const NetAddress& endpoint,
        std::uint32_t ack)
    {
        SendToEndpoint(socketHub, endpoint, WrapUdpPayload({}, 0, ack, true));
    }

    std::vector<NetAddress> GetEndpointsInRoom(
        int roomId,
        const HostEngine& hostEngine,
        const std::unordered_map<int, NetAddress>& playerToEndpoint)
    {
        std::vector<NetAddress> result;

        for (const auto& pair : playerToEndpoint)
        {
            int playerId = pair.first;
            const NetAddress& endpoint = pair.second;

            if (hostEngine.GetPlayerRoomId(playerId) == roomId)
            {
                result.push_back(endpoint);
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
        const std::unordered_map<int, NetAddress>& playerToEndpoint,
        int roomId,
        const std::vector<std::uint8_t>& payload)
    {
        std::vector<NetAddress> endpoints = GetEndpointsInRoom(roomId, hostEngine, playerToEndpoint);

        for (const NetAddress& endpoint : endpoints)
        {
            SendWrapped(socketHub, endpoint, payload);
        }
    }

    void BroadcastRoomSnapshot(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, NetAddress>& playerToEndpoint,
        int roomId)
    {
        RoomSnapshot snapshot = hostEngine.BuildRoomSnapshot(roomId);
        BroadcastToRoom(
            socketHub,
            hostEngine,
            playerToEndpoint,
            roomId,
            WireFormat::MakeWaitingRoomPacket(snapshot)
        );
    }

    void BroadcastMatchStarted(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, NetAddress>& playerToEndpoint,
        int roomId)
    {
        std::vector<NetAddress> endpoints = GetEndpointsInRoom(roomId, hostEngine, playerToEndpoint);

        for (const NetAddress& endpoint : endpoints)
        {
            auto endpointIt = std::find_if(
                playerToEndpoint.begin(),
                playerToEndpoint.end(),
                [&](const auto& pair)
                {
                    return pair.second == endpoint;
                });

            if (endpointIt == playerToEndpoint.end())
            {
                continue;
            }

            int playerId = endpointIt->first;
            const PuzzleData* puzzle = hostEngine.GetPuzzleForPlayer(playerId);
            if (puzzle != nullptr)
            {
                SendWrapped(socketHub, endpoint, WireFormat::MakeMatchStartedPacket(*puzzle));
            }
        }
    }

    void BroadcastLeaderboard(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, NetAddress>& playerToEndpoint,
        int roomId)
    {
        std::vector<NetAddress> endpoints = GetEndpointsInRoom(roomId, hostEngine, playerToEndpoint);
        if (endpoints.empty())
        {
            return;
        }

        auto endpointIt = std::find_if(
            playerToEndpoint.begin(),
            playerToEndpoint.end(),
            [&](const auto& pair)
            {
                return pair.second == endpoints[0];
            });

        if (endpointIt == playerToEndpoint.end())
        {
            return;
        }

        std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(endpointIt->first);
        PatchLeaderboardNames(hostEngine, leaderboard);

        BroadcastToRoom(
            socketHub,
            hostEngine,
            playerToEndpoint,
            roomId,
            WireFormat::MakeLeaderboardPacket(leaderboard)
        );
    }

    void BroadcastRoomPresenceState(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<int, NetAddress>& playerToEndpoint,
        int roomId,
        int anyPlayerInRoom)
    {
        BroadcastRoomSnapshot(socketHub, hostEngine, playerToEndpoint, roomId);

        std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(anyPlayerInRoom);
        PatchLeaderboardNames(hostEngine, leaderboard);

        BroadcastToRoom(
            socketHub,
            hostEngine,
            playerToEndpoint,
            roomId,
            WireFormat::MakeLeaderboardPacket(leaderboard)
        );
    }

    void SendError(
        SocketHub& socketHub,
        const NetAddress& endpoint,
        const std::string& text)
    {
        SendWrapped(socketHub, endpoint, WireFormat::MakeErrorPacket(text));
    }


    void BroadcastPublicRoomList(
        SocketHub& socketHub,
        const HostEngine& hostEngine,
        const std::unordered_map<NetAddress, bool, NetAddressHash>& endpointAuthenticated)
    {
        std::vector<PublicRoomInfo> rooms = hostEngine.GetPublicJoinableRooms();
        std::vector<std::uint8_t> payload = WireFormat::MakePublicRoomListResponsePacket(rooms);

        for (const auto& pair : endpointAuthenticated)
        {
            const NetAddress& endpoint = pair.first;
            bool authenticated = pair.second;

            if (authenticated)
            {
                SendWrapped(socketHub, endpoint, payload);
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

    bool SaveMatchHistoryAndBroadcastIfRoundEnded(
        SocketHub& socketHub,
        HostEngine& hostEngine,
        UserDatabase& userDb,
        const std::unordered_map<int, NetAddress>& playerToEndpoint,
        int roomId,
        int playerId)
    {
        if (roomId == 0)
        {
            return false;
        }

        std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(playerId);
        PatchLeaderboardNames(hostEngine, leaderboard);

        if (!HasAnyFinished(leaderboard))
        {
            return false;
        }

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
                avgMoveSeconds =
                    static_cast<double>(totalTimeSeconds) /
                    static_cast<double>(stat.moveCount);
            }

            bool ok = userDb.InsertMatchHistory(
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

            std::cout << "[HistoryInsert] user=" << profile->GetPlayerName()
                << " result=" << resultText
                << " ok=" << (ok ? "YES" : "NO") << "\n";
        }

        BroadcastToRoom(
            socketHub,
            hostEngine,
            playerToEndpoint,
            roomId,
            WireFormat::MakeRoundFinishedPacket(BuildRoundFinishedText(leaderboard))
        );

        return true;
    }
}

int main()
{
    SocketHub socketHub;
    HostEngine hostEngine;
    UserDatabase userDb;
    std::unordered_map<NetAddress, int, NetAddressHash> endpointToPlayer;
    std::unordered_map<int, NetAddress> playerToEndpoint;
    std::unordered_map<NetAddress, bool, NetAddressHash> endpointAuthenticated;
    std::unordered_map<int, std::chrono::steady_clock::time_point> playerLastActive;

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

            try
            {
                if (event.payload.size() < 9)
                {
                    continue;
                }

                ByteReader envReader(event.payload);
                ReliableEnvelope env = WireFormat::ReadReliableEnvelope(envReader);

                if (env.isAckOnly)
                {
                    continue;
                }

                if (env.sequence != 0)
                {
                    SendAckOnly(socketHub, event.from, env.sequence);
                }

                std::vector<std::uint8_t> payload(
                    event.payload.begin() + 9,
                    event.payload.end()
                );

                ByteReader reader(payload);
                PacketKind kind = WireFormat::ReadPacketKind(reader);

                auto playerMapIt = endpointToPlayer.find(event.from);
                if (playerMapIt != endpointToPlayer.end())
                {
                    int activePlayerId = playerMapIt->second;
                    playerLastActive[activePlayerId] = std::chrono::steady_clock::now();

                    ConnectionProfile* activeProfile = hostEngine.FindPlayerProfile(activePlayerId);
                    if (activeProfile != nullptr && !activeProfile->IsConnected())
                    {
                        activeProfile->SetConnected(true);

                        int activeRoomId = activeProfile->GetRoomId();
                        if (activeRoomId != 0)
                        {
                            BroadcastRoomPresenceState(
                                socketHub,
                                hostEngine,
                                playerToEndpoint,
                                activeRoomId,
                                activePlayerId
                            );
                        }
                    }
                }

                switch (kind)
                {

                case PacketKind::DiscoverServerRequest:
                {
                    ServerAnnounceData data{};
                    data.serverName = "Sudoku Rush Server";
                    data.port = 54000;

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeServerAnnouncePacket(data)
                    );
                    break;
                }

                case PacketKind::Heartbeat:
                {
                    break;
                }

                case PacketKind::ResumeMatchRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                    if (profile == nullptr)
                    {
                        SendError(socketHub, event.from, "Player profile not found.");
                        break;
                    }

                    int roomId = profile->GetRoomId();
                    if (roomId == 0)
                    {
                        SendError(socketHub, event.from, "No active room to resume.");
                        break;
                    }

                    if (!hostEngine.IsMatchRunningForPlayer(playerId))
                    {
                        SendError(socketHub, event.from, "Match is no longer running.");
                        break;
                    }

                    profile->SetConnected(true);
                    playerLastActive[playerId] = std::chrono::steady_clock::now();

                    const PuzzleData* puzzle = hostEngine.GetPuzzleForPlayer(playerId);
                    if (puzzle != nullptr)
                    {
                        SendWrapped(
                            socketHub,
                            event.from,
                            WireFormat::MakeMatchStartedPacket(*puzzle)
                        );
                    }

                    std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(playerId);
                    PatchLeaderboardNames(hostEngine, leaderboard);

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeLeaderboardPacket(leaderboard)
                    );

                    HintState hintState = hostEngine.GetHintStateForPlayer(playerId);
                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeHintUnlockedPacket(hintState)
                    );

                    BroadcastRoomPresenceState(
                        socketHub,
                        hostEngine,
                        playerToEndpoint,
                        roomId,
                        playerId
                    );

                    break;
                }

                case PacketKind::SignUpRequest:
                {
                    AuthRequest request = WireFormat::ReadAuthRequest(reader);

                    AuthResponseData response{};

                    if (request.playerName.empty() || request.password.empty())
                    {
                        response.success = false;
                        response.text = "Username and password are required.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (userDb.UserExists(request.playerName))
                    {
                        response.success = false;
                        response.text = "Username already exists.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (!userDb.RegisterUser(request.playerName, request.password))
                    {
                        response.success = false;
                        response.text = "Failed to register user.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    int playerId = 0;
                    auto mapIt = endpointToPlayer.find(event.from);

                    if (mapIt == endpointToPlayer.end())
                    {
                        playerId = hostEngine.RegisterPlayer(request.playerName);
                        endpointToPlayer[event.from] = playerId;
                        playerToEndpoint[playerId] = event.from;
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

                    endpointAuthenticated[event.from] = true;

                    playerLastActive[playerId] = std::chrono::steady_clock::now();

                    ConnectionProfile* signedUpProfile = hostEngine.FindPlayerProfile(playerId);
                    if (signedUpProfile != nullptr)
                    {
                        signedUpProfile->SetConnected(true);
                    }


                    response.success = true;
                    response.playerId = playerId;
                    response.text = "Sign up successful.";

                    SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
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
                        SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (!userDb.UserExists(request.playerName))
                    {
                        response.success = false;
                        response.text = "User does not exist.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    if (!userDb.VerifyUser(request.playerName, request.password))
                    {
                        response.success = false;
                        response.text = "Wrong password.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                        break;
                    }

                    int playerId = 0;
                    auto mapIt = endpointToPlayer.find(event.from);

                    if (mapIt == endpointToPlayer.end())
                    {
                        playerId = hostEngine.RegisterPlayer(request.playerName);
                        endpointToPlayer[event.from] = playerId;
                        playerToEndpoint[playerId] = event.from;
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

                    endpointAuthenticated[event.from] = true;

                    playerLastActive[playerId] = std::chrono::steady_clock::now();

                    ConnectionProfile* signedInProfile = hostEngine.FindPlayerProfile(playerId);
                    if (signedInProfile != nullptr)
                    {
                        signedInProfile->SetConnected(true);
                    }

                    response.success = true;
                    response.playerId = playerId;
                    response.text = "Login successful.";

                    SendWrapped(socketHub, event.from, WireFormat::MakeAuthResponsePacket(response));
                    break;
                }

                case PacketKind::CreateRoomRequest:
                {
                    CreateRoomRequest request = WireFormat::ReadCreateRoomRequest(reader);

                    auto authIt = endpointAuthenticated.find(event.from);
                    if (authIt == endpointAuthenticated.end() || !authIt->second)
                    {
                        RoomJoinedResponseData response{};
                        response.success = false;
                        response.text = "Please log in first.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeRoomJoinedResponsePacket(response));
                        break;
                    }

                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.CreateRoomForPlayer(playerId, request.maxPlayers, request.visibility);

                    RoomJoinedResponseData response{};
                    response.success = (roomId != 0);
                    response.playerId = playerId;
                    response.roomId = roomId;
                    response.text = response.success ? "Room created." : "Failed to create room.";

                    SendWrapped(socketHub, event.from, WireFormat::MakeRoomJoinedResponsePacket(response));

                    if (response.success)
                    {
                        BroadcastRoomSnapshot(socketHub, hostEngine, playerToEndpoint, roomId);
                        BroadcastPublicRoomList(socketHub, hostEngine, endpointAuthenticated);
                    }
                    break;
                }

                case PacketKind::JoinRoomRequest:
                {
                    JoinRoomRequest request = WireFormat::ReadJoinRoomRequest(reader);

                    auto authIt = endpointAuthenticated.find(event.from);
                    if (authIt == endpointAuthenticated.end() || !authIt->second)
                    {
                        RoomJoinedResponseData response{};
                        response.success = false;
                        response.text = "Please log in first.";
                        SendWrapped(socketHub, event.from, WireFormat::MakeRoomJoinedResponsePacket(response));
                        break;
                    }

                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    bool joined = hostEngine.JoinRoomForPlayer(playerId, request.roomId);

                    RoomJoinedResponseData response{};
                    response.success = joined;
                    response.playerId = playerId;
                    response.roomId = joined ? request.roomId : 0;
                    response.text = joined ? "Joined room." : "Failed to join room.";

                    SendWrapped(socketHub, event.from, WireFormat::MakeRoomJoinedResponsePacket(response));

                    if (joined)
                    {
                        BroadcastRoomSnapshot(socketHub, hostEngine, playerToEndpoint, request.roomId);
                        BroadcastPublicRoomList(socketHub, hostEngine, endpointAuthenticated);
                    }
                    break;
                }

                case PacketKind::ReadyToggleRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    bool ready = reader.ReadBool();

                    if (!hostEngine.SetPlayerReady(playerId, ready))
                    {
                        SendError(socketHub, event.from, "Failed to update ready state.");
                        break;
                    }

                    int roomId = hostEngine.GetPlayerRoomId(playerId);
                    BroadcastRoomSnapshot(socketHub, hostEngine, playerToEndpoint, roomId);

                    if (hostEngine.TryStartMatch(roomId))
                    {
                        BroadcastMatchStarted(socketHub, hostEngine, playerToEndpoint, roomId);
                        BroadcastLeaderboard(socketHub, hostEngine, playerToEndpoint, roomId);
                    }
                    break;
                }

                case PacketKind::SubmitCellRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
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

                    SendWrapped(
                        socketHub,
                        event.from,
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
                            playerToEndpoint,
                            roomId,
                            WireFormat::MakeCellClaimUpdatePacket(claim)
                        );
                    }

                    std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(playerId);
                    PatchLeaderboardNames(hostEngine, leaderboard);

                    BroadcastToRoom(
                        socketHub,
                        hostEngine,
                        playerToEndpoint,
                        roomId,
                        WireFormat::MakeLeaderboardPacket(leaderboard)
                    );

                    HintState hintState = hostEngine.GetHintStateForPlayer(playerId);
                    if (hintState.unlocked)
                    {
                        SendWrapped(socketHub, event.from, WireFormat::MakeHintUnlockedPacket(hintState));
                    }

                    SaveMatchHistoryAndBroadcastIfRoundEnded(
                        socketHub,
                        hostEngine,
                        userDb,
                        playerToEndpoint,
                        roomId,
                        playerId
                    );

                    break;
                }

                case PacketKind::TogglePlayAgainRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);
                    bool playAgain = reader.ReadBool();

                    if (!hostEngine.SetPlayerPlayAgain(playerId, playAgain))
                    {
                        SendError(socketHub, event.from, "Failed to update replay vote.");
                        break;
                    }

                    if (hostEngine.TryStartReplay(roomId))
                    {
                        BroadcastMatchStarted(socketHub, hostEngine, playerToEndpoint, roomId);
                        BroadcastLeaderboard(socketHub, hostEngine, playerToEndpoint, roomId);
                    }
                    break;
                }

                case PacketKind::UseHintRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    HintRequest request = WireFormat::ReadHintRequest(reader);
                    CellResult hintResult = hostEngine.UseHint(playerId, request.row, request.col);

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeCellCheckedPacket(hintResult)
                    );

                    HintState hintState = hostEngine.GetHintStateForPlayer(playerId);
                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeHintUnlockedPacket(hintState)
                    );

                    std::vector<PlayerScoreInfo> leaderboard = hostEngine.GetLeaderboardForPlayer(playerId);
                    PatchLeaderboardNames(hostEngine, leaderboard);

                    BroadcastToRoom(
                        socketHub,
                        hostEngine,
                        playerToEndpoint,
                        roomId,
                        WireFormat::MakeLeaderboardPacket(leaderboard)
                    );

                    SaveMatchHistoryAndBroadcastIfRoundEnded(
                        socketHub,
                        hostEngine,
                        userDb,
                        playerToEndpoint,
                        roomId,
                        playerId
                    );

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

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeSearchUserResponsePacket(result)
                    );
                    break;
                }

                case PacketKind::SendFriendRequest:
                {
                    std::string fromUser = GetUsernameForEndpoint(event.from, endpointToPlayer, hostEngine);
                    if (fromUser.empty())
                    {
                        SendError(socketHub, event.from, "Not authenticated.");
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

                            NetAddress* targetEndpoint = FindEndpointByUsername(targetUser, playerToEndpoint, hostEngine);
                            if (targetEndpoint != nullptr)
                            {
                                FriendInfo info{};
                                info.username = fromUser;
                                std::cout << "[FriendRequestResult] " << responseText << "\n";
                                SendWrapped(
                                    socketHub,
                                    *targetEndpoint,
                                    WireFormat::MakeFriendRequestReceivedPacket(info)
                                );
                            }
                        }
                        else
                        {
                            responseText = "Failed to send friend request.";
                        }
                    }

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeFriendRequestResponsePacket(responseText)
                    );
                    break;
                }

                case PacketKind::RespondFriendRequest:
                {
                    std::string currentUser = GetUsernameForEndpoint(event.from, endpointToPlayer, hostEngine);
                    if (currentUser.empty())
                    {
                        SendError(socketHub, event.from, "Not authenticated.");
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

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeFriendRequestResponsePacket(responseText)
                    );

                    std::vector<FriendInfo> updatedFriends = userDb.GetFriendsForUser(currentUser);
                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeFriendsListResponsePacket(updatedFriends)
                    );

                    std::vector<FriendInfo> updatedRequests = userDb.GetPendingFriendRequestsForUser(currentUser);
                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakePendingFriendRequestsResponsePacket(updatedRequests)
                    );

                    if (decision.accept)
                    {
                        NetAddress* otherEndpoint = FindEndpointByUsername(
                            decision.fromUsername,
                            playerToEndpoint,
                            hostEngine
                        );

                        if (otherEndpoint != nullptr)
                        {
                            std::vector<FriendInfo> otherUpdatedFriends =
                                userDb.GetFriendsForUser(decision.fromUsername);

                            SendWrapped(
                                socketHub,
                                *otherEndpoint,
                                WireFormat::MakeFriendsListResponsePacket(otherUpdatedFriends)
                            );

                            SendWrapped(
                                socketHub,
                                *otherEndpoint,
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
                    std::string currentUser = GetUsernameForEndpoint(event.from, endpointToPlayer, hostEngine);
                    if (currentUser.empty())
                    {
                        SendError(socketHub, event.from, "Not authenticated.");
                        break;
                    }

                    std::vector<FriendInfo> friends = userDb.GetFriendsForUser(currentUser);
                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakeFriendsListResponsePacket(friends)
                    );
                    break;
                }

                case PacketKind::RequestPendingFriendRequests:
                {
                    std::string currentUser = GetUsernameForEndpoint(event.from, endpointToPlayer, hostEngine);
                    if (currentUser.empty())
                    {
                        SendError(socketHub, event.from, "Not authenticated.");
                        break;
                    }

                    std::vector<FriendInfo> requests = userDb.GetPendingFriendRequestsForUser(currentUser);
                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakePendingFriendRequestsResponsePacket(requests)
                    );
                    break;
                }

                case PacketKind::SendPrivateMessage:
                {
                    std::string fromUser = GetUsernameForEndpoint(event.from, endpointToPlayer, hostEngine);
                    if (fromUser.empty())
                    {
                        SendError(socketHub, event.from, "Not authenticated.");
                        break;
                    }

                    PrivateMessagePayload payload = WireFormat::ReadPrivateMessagePayload(reader);

                    if (payload.targetUsername.empty() || payload.text.empty())
                    {
                        SendError(socketHub, event.from, "Invalid private message.");
                        break;
                    }

                    if (!userDb.AreFriends(fromUser, payload.targetUsername))
                    {
                        SendError(socketHub, event.from, "You can only message friends.");
                        break;
                    }

                    PrivateChatMessage msg{};
                    msg.senderName = fromUser;
                    msg.receiverName = payload.targetUsername;
                    msg.text = payload.text;
                    msg.timestamp = GetCurrentDateTimeString();

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakePrivateMessageReceivedPacket(msg)
                    );

                    NetAddress* targetEndpoint = FindEndpointByUsername(payload.targetUsername, playerToEndpoint, hostEngine);
                    if (targetEndpoint != nullptr)
                    {
                        SendWrapped(
                            socketHub,
                            *targetEndpoint,
                            WireFormat::MakePrivateMessageReceivedPacket(msg)
                        );
                    }

                    break;
                }

                case PacketKind::HistoryRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    const ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                    if (profile == nullptr)
                    {
                        SendError(socketHub, event.from, "Player profile not found.");
                        break;
                    }

                    std::vector<MatchHistoryRow> history = userDb.GetMatchHistoryForUser(profile->GetPlayerName());
                    SendWrapped(socketHub, event.from, WireFormat::MakeHistoryResponsePacket(history));
                    break;
                }

                case PacketKind::LeaveRoomRequest:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    if (roomId == 0)
                    {
                        LeaveRoomResponseData response{};
                        response.success = false;
                        response.text = "Not in a room.";
                        SendWrapped(socketHub, event.from,
                            WireFormat::MakeLeaveRoomResponsePacket(response));
                        break;
                    }

                    bool isHost = hostEngine.IsHostOfRoom(playerId, roomId);

                    if (isHost)
                    {
                        std::vector<int> playerIds = hostEngine.GetPlayersInRoom(roomId);
                        std::vector<NetAddress> roomEndpoints = GetEndpointsForPlayers(playerIds, playerToEndpoint);

                        hostEngine.CloseRoom(roomId);

                        for (const NetAddress& endpoint : roomEndpoints)
                        {
                            SendWrapped(socketHub, endpoint,
                                WireFormat::MakeRoomClosedNoticePacket("Host left. Room closed."));
                        }

                        BroadcastPublicRoomList(socketHub, hostEngine, endpointAuthenticated);
                    }
                    else
                    {
                        bool removed = hostEngine.RemovePlayerFromRoom(playerId);

                        LeaveRoomResponseData response{};
                        response.success = removed;
                        response.text = removed ? "Left room." : "Failed to leave room.";

                        SendWrapped(socketHub, event.from,
                            WireFormat::MakeLeaveRoomResponsePacket(response));

                        if (removed)
                        {
                            BroadcastRoomSnapshot(socketHub, hostEngine, playerToEndpoint, roomId);
                            BroadcastPublicRoomList(socketHub, hostEngine, endpointAuthenticated);
                        }
                    }

                    break;
                }

                case PacketKind::SendRoomChatMessage:
                {
                    auto mapIt = endpointToPlayer.find(event.from);
                    if (mapIt == endpointToPlayer.end())
                    {
                        SendError(socketHub, event.from, "Player not registered.");
                        break;
                    }

                    int playerId = mapIt->second;
                    int roomId = hostEngine.GetPlayerRoomId(playerId);

                    if (roomId == 0)
                    {
                        SendError(socketHub, event.from, "Not in a room.");
                        break;
                    }

                    const ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                    if (profile == nullptr)
                    {
                        SendError(socketHub, event.from, "Player profile not found.");
                        break;
                    }

                    ChatMessage msg{};
                    msg.senderName = profile->GetPlayerName();
                    msg.text = reader.ReadString();
                    msg.timestamp = GetCurrentDateTimeString();

                    BroadcastToRoom(
                        socketHub,
                        hostEngine,
                        playerToEndpoint,
                        roomId,
                        WireFormat::MakeRoomChatMessagePacket(msg)
                    );
                    break;
                }

                case PacketKind::RequestPublicRoomList:
                {
                    std::vector<PublicRoomInfo> rooms = hostEngine.GetPublicJoinableRooms();

                    SendWrapped(
                        socketHub,
                        event.from,
                        WireFormat::MakePublicRoomListResponsePacket(rooms)
                    );
                    break;
                }

                default:
                    SendError(socketHub, event.from, "Unhandled packet kind.");
                    break;
                }
            }
            catch (const std::exception& ex)
            {
                SendError(socketHub, event.from, std::string("Packet decode error: ") + ex.what());
            }
        }

        {
            auto now = std::chrono::steady_clock::now();

            for (auto& pair : playerLastActive)
            {
                int playerId = pair.first;
                auto lastActive = pair.second;

                const ConnectionProfile* profile = hostEngine.FindPlayerProfile(playerId);
                if (profile == nullptr)
                {
                    continue;
                }

                int roomId = profile->GetRoomId();
                if (roomId == 0)
                {
                    continue;
                }

                long long idleMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - lastActive).count();

                bool matchRunning = hostEngine.IsMatchRunningForPlayer(playerId);

                ConnectionProfile* mutableProfile = hostEngine.FindPlayerProfile(playerId);
                if (mutableProfile == nullptr)
                {
                    continue;
                }

                if (!matchRunning)
                {
                    if (idleMs >= kWaitingRoomIdleMs)
                    {
                        auto endpointIt = playerToEndpoint.find(playerId);
                        if (endpointIt == playerToEndpoint.end())
                        {
                            continue;
                        }

                        NetAddress endpoint = endpointIt->second;
                        bool isHost = hostEngine.IsHostOfRoom(playerId, roomId);

                        if (isHost)
                        {
                            std::vector<int> playerIds = hostEngine.GetPlayersInRoom(roomId);
                            std::vector<NetAddress> roomEndpoints = GetEndpointsForPlayers(playerIds, playerToEndpoint);

                            hostEngine.CloseRoom(roomId);

                            for (const NetAddress& roomEndpoint : roomEndpoints)
                            {
                                SendWrapped(
                                    socketHub,
                                    roomEndpoint,
                                    WireFormat::MakeRoomClosedNoticePacket("Host inactive. Room closed.")
                                );
                            }

                            BroadcastPublicRoomList(socketHub, hostEngine, endpointAuthenticated);
                        }
                        else
                        {
                            bool removed = hostEngine.RemovePlayerFromRoom(playerId);

                            LeaveRoomResponseData response{};
                            response.success = removed;
                            response.text = removed
                                ? "Removed from room due to inactivity."
                                : "Failed to remove inactive player.";

                            SendWrapped(
                                socketHub,
                                endpoint,
                                WireFormat::MakeLeaveRoomResponsePacket(response)
                            );

                            if (removed)
                            {
                                BroadcastRoomSnapshot(socketHub, hostEngine, playerToEndpoint, roomId);
                                BroadcastPublicRoomList(socketHub, hostEngine, endpointAuthenticated);
                            }
                        }

                        mutableProfile->SetConnected(false);
                    }
                }
                else
                {
                    if (idleMs >= kPlayingTimeoutMs && mutableProfile->IsConnected())
                    {
                        mutableProfile->SetConnected(false);

                        auto endpointIt = playerToEndpoint.find(playerId);
                        if (endpointIt != playerToEndpoint.end())
                        {
                            SendWrapped(
                                socketHub,
                                endpointIt->second,
                                WireFormat::MakeErrorPacket(
                                    "Disconnected due to inactivity. Click Resume if the match is still running."
                                )
                            );
                        }

                        BroadcastRoomPresenceState(
                            socketHub,
                            hostEngine,
                            playerToEndpoint,
                            roomId,
                            playerId
                        );
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    }

    socketHub.Stop();
    return 0;
}