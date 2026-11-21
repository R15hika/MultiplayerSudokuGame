#include "ServerBridge.h"

#include "../Shared/WireFormat.h"
#pragma comment(lib, "Ws2_32.lib")

ServerBridge::ServerBridge()
{
}

ServerBridge::~ServerBridge()
{
    Disconnect();
}

bool ServerBridge::InitializeWinsock()
{
    if (mWinsockInitialized)
    {
        return true;
    }

    WSADATA wsaData{};
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return false;
    }

    mWinsockInitialized = true;
    return true;
}

void ServerBridge::ShutdownWinsock()
{
    if (!mWinsockInitialized)
    {
        return;
    }

    ::WSACleanup();
    mWinsockInitialized = false;
}

bool ServerBridge::SetSocketNonBlocking(SOCKET socketHandle)
{
    u_long nonBlocking = 1;
    return ::ioctlsocket(socketHandle, FIONBIO, &nonBlocking) == 0;
}

bool ServerBridge::EnableBroadcast(SOCKET socketHandle)
{
    BOOL broadcast = TRUE;
    return ::setsockopt(
        socketHandle,
        SOL_SOCKET,
        SO_BROADCAST,
        reinterpret_cast<const char*>(&broadcast),
        sizeof(broadcast)) == 0;
}

bool ServerBridge::Connect(unsigned short port)
{
    Disconnect();

    if (!InitializeWinsock())
    {
        return false;
    }

    mSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mSocket == INVALID_SOCKET)
    {
        ShutdownWinsock();
        return false;
    }

    if (!SetSocketNonBlocking(mSocket))
    {
        Disconnect();
        return false;
    }

    if (!EnableBroadcast(mSocket))
    {
        Disconnect();
        return false;
    }

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(0);

    if (::bind(mSocket, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) == SOCKET_ERROR)
    {
        Disconnect();
        return false;
    }

    mBroadcastAddr = {};
    mBroadcastAddr.sin_family = AF_INET;
    mBroadcastAddr.sin_port = htons(port);
    mBroadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    mServerAddr = {};
    mPort = port;
    mHost.clear();
    mConnected = true;
    mServerDiscovered = false;
    mLastDiscoverSent = std::chrono::steady_clock::time_point{};

    return true;
}

void ServerBridge::Disconnect()
{
    mConnected = false;
    mAssignedPlayerId = 0;
    mAssignedRoomId = 0;
    mHost.clear();
    mPort = 0;
    mPendingReliable.clear();
    mSeenIncoming.clear();

    while (!mInbox.empty())
    {
        mInbox.pop();
    }

    if (mSocket != INVALID_SOCKET)
    {
        ::closesocket(mSocket);
        mSocket = INVALID_SOCKET;
    }

    ShutdownWinsock();
}

bool ServerBridge::IsConnected() const
{
    return mConnected;
}

bool ServerBridge::HasDiscoveredServer() const
{
    return mServerDiscovered;
}

void ServerBridge::SendDiscoverServerRequest()
{
    if (!mConnected || mSocket == INVALID_SOCKET || mServerDiscovered)
    {
        return;
    }

    std::vector<std::uint8_t> payload = WireFormat::MakeDiscoverServerPacket();

    ByteWriter writer;
    writer.WriteUInt32(0);
    writer.WriteUInt32(0);
    writer.WriteBool(false);

    for (std::uint8_t b : payload)
    {
        writer.WriteUInt8(b);
    }

    std::vector<std::uint8_t> bytes = writer.GetBuffer();

    ::sendto(
        mSocket,
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()),
        0,
        reinterpret_cast<const sockaddr*>(&mBroadcastAddr),
        sizeof(mBroadcastAddr)
    );
}

bool ServerBridge::SendAck(std::uint32_t ack)
{
    if (!mConnected || mSocket == INVALID_SOCKET)
    {
        return false;
    }

    ByteWriter writer;
    writer.WriteUInt32(0);
    writer.WriteUInt32(ack);
    writer.WriteBool(true);

    const std::vector<std::uint8_t>& payload = writer.GetBuffer();

    int sent = ::sendto(
        mSocket,
        reinterpret_cast<const char*>(payload.data()),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<const sockaddr*>(&mServerAddr),
        sizeof(mServerAddr)
    );

    return sent == static_cast<int>(payload.size());
}

bool ServerBridge::SendReliablePayload(const std::vector<std::uint8_t>& payload)
{
    if (!mConnected || mSocket == INVALID_SOCKET || !mServerDiscovered)
    {
        return false;
    }

    std::uint32_t seq = mNextSequence++;

    ByteWriter writer;
    writer.WriteUInt32(seq);
    writer.WriteUInt32(0);
    writer.WriteBool(false);

    for (std::uint8_t b : payload)
    {
        writer.WriteUInt8(b);
    }

    std::vector<std::uint8_t> bytes = writer.GetBuffer();

    int sent = ::sendto(
        mSocket,
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()),
        0,
        reinterpret_cast<const sockaddr*>(&mServerAddr),
        sizeof(mServerAddr)
    );

    if (sent == static_cast<int>(bytes.size()))
    {
        PendingPacket pp{};
        pp.bytes = bytes;
        pp.lastSent = std::chrono::steady_clock::now();
        mPendingReliable[seq] = std::move(pp);
        return true;
    }

    return false;
}

void ServerBridge::PollResends()
{
    if (!mConnected || mSocket == INVALID_SOCKET)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    if (!mServerDiscovered)
    {
        if (mLastDiscoverSent.time_since_epoch().count() == 0 ||
            std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastDiscoverSent).count() >= 1000)
        {
            SendDiscoverServerRequest();
            mLastDiscoverSent = now;
        }

        return;
    }

    for (auto& pair : mPendingReliable)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second.lastSent).count();
        if (elapsed >= 100)
        {
            ::sendto(
                mSocket,
                reinterpret_cast<const char*>(pair.second.bytes.data()),
                static_cast<int>(pair.second.bytes.size()),
                0,
                reinterpret_cast<const sockaddr*>(&mServerAddr),
                sizeof(mServerAddr)
            );

            pair.second.lastSent = now;
        }
    }
}

void ServerBridge::PushMessage(const BridgeMessage& msg)
{
    mInbox.push(msg);
}

bool ServerBridge::HasPendingMessage() const
{
    return !mInbox.empty();
}

BridgeMessage ServerBridge::PopNextMessage()
{
    if (mInbox.empty())
    {
        return BridgeMessage{};
    }

    BridgeMessage msg = mInbox.front();
    mInbox.pop();
    return msg;
}

int ServerBridge::GetAssignedPlayerId() const
{
    return mAssignedPlayerId;
}

int ServerBridge::GetAssignedRoomId() const
{
    return mAssignedRoomId;
}

void ServerBridge::ClearRoomAssignment()
{
    mAssignedRoomId = 0;
    mAssignedPlayerId = 0;
}

void ServerBridge::DecodePacket(const std::vector<std::uint8_t>& payload)
{
    try
    {
        ByteReader reader(payload);
        PacketKind kind = WireFormat::ReadPacketKind(reader);

        BridgeMessage msg{};
        msg.kind = kind;

        switch (kind)
        {

        case PacketKind::ServerAnnounce:
        {
            ServerAnnounceData announce = WireFormat::ReadServerAnnounce(reader);
            msg.text = announce.serverName;
            break;
        }

        case PacketKind::AuthResponse:
            msg.authResponse = WireFormat::ReadAuthResponse(reader);
            if (msg.authResponse.success)
            {
                mAssignedPlayerId = msg.authResponse.playerId;
            }
            break;

        case PacketKind::RoomJoinedResponse:
            msg.roomJoin = WireFormat::ReadRoomJoinedResponse(reader);
            if (msg.roomJoin.success)
            {
                mAssignedPlayerId = msg.roomJoin.playerId;
                mAssignedRoomId = msg.roomJoin.roomId;
            }
            break;

        case PacketKind::LeaveRoomResponse:
            msg.leaveRoomResponse = WireFormat::ReadLeaveRoomResponse(reader);
            msg.text = msg.leaveRoomResponse.text;
            break;

        case PacketKind::RoomClosedNotice:
            msg.text = reader.ReadString();
            break;

        case PacketKind::WaitingRoomUpdate:
            msg.roomSnapshot = WireFormat::ReadRoomSnapshot(reader);
            break;

        case PacketKind::MatchStarted:
            msg.puzzleData = WireFormat::ReadPuzzleData(reader);
            break;

        case PacketKind::CellCheckedResponse:
            msg.cellResult = WireFormat::ReadCellResult(reader);
            break;

        case PacketKind::CellClaimUpdateNotice:
            msg.cellClaim = WireFormat::ReadCellClaimUpdate(reader);
            break;

        case PacketKind::LeaderboardUpdate:
            msg.leaderboard = WireFormat::ReadPlayerScoreList(reader);
            break;

        case PacketKind::HintUnlockedNotice:
            msg.hintState = WireFormat::ReadHintState(reader);
            break;

        case PacketKind::RoomChatMessageReceived:
            msg.chatMessage = WireFormat::ReadChatMessage(reader);
            break;

        case PacketKind::SearchUserResponse:
            msg.userSearchResult = WireFormat::ReadUserSearchResult(reader);
            msg.text = msg.userSearchResult.text;
            break;

        case PacketKind::FriendRequestReceived:
        {
            FriendInfo info = WireFormat::ReadFriendInfo(reader);
            msg.text = info.username;
            break;
        }

        case PacketKind::FriendRequestResponse:
            msg.text = reader.ReadString();
            break;

        case PacketKind::FriendsListResponse:
            msg.friendList = WireFormat::ReadFriendList(reader);
            break;

        case PacketKind::PendingFriendRequestsResponse:
            msg.pendingFriendList = WireFormat::ReadFriendList(reader);
            break;

        case PacketKind::PrivateMessageReceived:
            msg.privateChatMessage = WireFormat::ReadPrivateChatMessage(reader);
            break;

        case PacketKind::PublicRoomListResponse:
            msg.publicRooms = WireFormat::ReadPublicRoomList(reader);
            break;

        case PacketKind::HistoryResponse:
            msg.historyRows = WireFormat::ReadMatchHistoryList(reader);
            break;

        case PacketKind::RoundFinishedNotice:
            msg.text = reader.ReadString();
            break;

        case PacketKind::ErrorNotice:
            msg.text = reader.ReadString();
            break;

        default:
            break;
        }

        PushMessage(msg);
    }
    catch (...)
    {
        BridgeMessage msg{};
        msg.kind = PacketKind::ErrorNotice;
        msg.text = "Failed to decode server packet.";
        PushMessage(msg);
    }
}

void ServerBridge::PollIncoming()
{
    if (!mConnected || mSocket == INVALID_SOCKET)
    {
        return;
    }

    std::uint8_t tempBuffer[4096]{};

    while (true)
    {
        sockaddr_in from{};
        int fromLen = sizeof(from);

        int bytesRead = ::recvfrom(
            mSocket,
            reinterpret_cast<char*>(tempBuffer),
            4096,
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (bytesRead > 0)
        {
            if (bytesRead < 9)
            {
                continue;
            }

            std::vector<std::uint8_t> bytes(tempBuffer, tempBuffer + bytesRead);
            ByteReader envReader(bytes);

            ReliableEnvelope env{};
            env.sequence = envReader.ReadUInt32();
            env.ack = envReader.ReadUInt32();
            env.isAckOnly = envReader.ReadBool();

            if (env.ack != 0)
            {
                mPendingReliable.erase(env.ack);
            }

            if (env.isAckOnly)
            {
                continue;
            }

            if (env.sequence != 0)
            {
                SendAck(env.sequence);

                if (mSeenIncoming.find(env.sequence) != mSeenIncoming.end())
                {
                    continue;
                }

                mSeenIncoming[env.sequence] = true;
            }

            std::vector<std::uint8_t> payload(
                bytes.begin() + 9,
                bytes.end()
            );

            try
            {
                ByteReader probe(payload);
                PacketKind kind = WireFormat::ReadPacketKind(probe);

                if (kind == PacketKind::ServerAnnounce)
                {
                    mServerAddr = from;
                    mServerDiscovered = true;
                }
            }
            catch (...)
            {
            }

            DecodePacket(payload);
        }
        else
        {
            int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                break;
            }

            break;
        }
    }
}

void ServerBridge::SendSignUpRequest(const std::string& playerName, const std::string& password)
{
    AuthRequest request{};
    request.playerName = playerName;
    request.password = password;
    SendReliablePayload(WireFormat::MakeSignUpPacket(request));
}

void ServerBridge::SendSignInRequest(const std::string& playerName, const std::string& password)
{
    AuthRequest request{};
    request.playerName = playerName;
    request.password = password;
    SendReliablePayload(WireFormat::MakeSignInPacket(request));
}

void ServerBridge::SendCreateRoomRequest(const CreateRoomRequest& request)
{
    SendReliablePayload(WireFormat::MakeCreateRoomPacket(request));
}

void ServerBridge::SendJoinRoomRequest(int roomId)
{
    JoinRoomRequest request{};
    request.roomId = roomId;
    SendReliablePayload(WireFormat::MakeJoinRoomPacket(request));
}

void ServerBridge::SendReadyToggleRequest(bool ready)
{
    SendReliablePayload(WireFormat::MakeReadyTogglePacket(ready));
}

void ServerBridge::SendSubmitCellRequest(int row, int col, int value)
{
    CellInputRequest request{};
    request.row = row;
    request.col = col;
    request.value = value;
    SendReliablePayload(WireFormat::MakeSubmitCellPacket(request));
}

void ServerBridge::SendPlayAgainRequest(bool playAgain)
{
    SendReliablePayload(WireFormat::MakePlayAgainPacket(playAgain));
}

void ServerBridge::SendUseHintRequest(int row, int col)
{
    HintRequest request{};
    request.row = row;
    request.col = col;
    SendReliablePayload(WireFormat::MakeUseHintPacket(request));
}

void ServerBridge::SendLeaveRoomRequest()
{
    SendReliablePayload(WireFormat::MakeLeaveRoomRequestPacket());
}

void ServerBridge::SendHistoryRequest()
{
    SendReliablePayload(WireFormat::MakeHistoryRequestPacket());
}

void ServerBridge::SendRequestPublicRoomList()
{
    SendReliablePayload(WireFormat::MakeRequestPublicRoomListPacket());
}

void ServerBridge::SendRoomChatMessage(const std::string& text)
{
    SendReliablePayload(WireFormat::MakeSendRoomChatPacket(text));
}

void ServerBridge::SendSearchUserRequest(const std::string& username)
{
    SendReliablePayload(WireFormat::MakeSearchUserPacket(username));
}

void ServerBridge::SendFriendRequest(const std::string& username)
{
    FriendRequestPayload payload{};
    payload.targetUsername = username;
    SendReliablePayload(WireFormat::MakeSendFriendRequestPacket(payload));
}

void ServerBridge::SendFriendRequestResponse(const std::string& fromUsername, bool accept)
{
    FriendRequestDecision decision{};
    decision.fromUsername = fromUsername;
    decision.accept = accept;
    SendReliablePayload(WireFormat::MakeRespondFriendRequestPacket(decision));
}

void ServerBridge::SendRequestFriendsList()
{
    SendReliablePayload(WireFormat::MakeRequestFriendsListPacket());
}

void ServerBridge::SendRequestPendingFriendRequests()
{
    SendReliablePayload(WireFormat::MakeRequestPendingFriendRequestsPacket());
}

void ServerBridge::SendHeartbeat()
{
    SendReliablePayload(WireFormat::MakeHeartbeatPacket());
}

void ServerBridge::SendResumeMatchRequest()
{
    SendReliablePayload(WireFormat::MakeResumeMatchRequestPacket());
}

void ServerBridge::SendPrivateMessage(const std::string& targetUsername, const std::string& text)
{
    PrivateMessagePayload payload{};
    payload.targetUsername = targetUsername;
    payload.text = text;
    SendReliablePayload(WireFormat::MakeSendPrivateMessagePacket(payload));
}