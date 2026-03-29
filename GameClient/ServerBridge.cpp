#include "ServerBridge.h"

#include "../Shared/WireFormat.h"
#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr std::uint32_t kHeaderSize = 4;
    constexpr int kTempReadSize = 4096;

    void WriteLengthPrefix(std::vector<std::uint8_t>& buffer, std::uint32_t length)
    {
        buffer.push_back(static_cast<std::uint8_t>((length >> 24) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((length >> 16) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>(length & 0xFF));
    }

    std::uint32_t ReadLengthPrefix(const std::vector<std::uint8_t>& buffer, std::size_t offset)
    {
        return (static_cast<std::uint32_t>(buffer[offset]) << 24) |
            (static_cast<std::uint32_t>(buffer[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(buffer[offset + 2]) << 8) |
            (static_cast<std::uint32_t>(buffer[offset + 3]));
    }
}

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

bool ServerBridge::Connect(const std::string& host, unsigned short port)
{
    Disconnect();

    if (!InitializeWinsock())
    {
        return false;
    }

    mSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mSocket == INVALID_SOCKET)
    {
        ShutdownWinsock();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1)
    {
        Disconnect();
        return false;
    }

    if (::connect(mSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        Disconnect();
        return false;
    }

    if (!SetSocketNonBlocking(mSocket))
    {
        Disconnect();
        return false;
    }

    mHost = host;
    mPort = port;
    mConnected = true;
    return true;
}

void ServerBridge::Disconnect()
{
    mConnected = false;
    mAssignedPlayerId = 0;
    mAssignedRoomId = 0;
    mHost.clear();
    mPort = 0;
    mReceiveBuffer.clear();

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

bool ServerBridge::SendAll(const std::uint8_t* data, int byteCount)
{
    int totalSent = 0;

    while (totalSent < byteCount)
    {
        int sent = ::send(
            mSocket,
            reinterpret_cast<const char*>(data + totalSent),
            byteCount - totalSent,
            0
        );

        if (sent == SOCKET_ERROR)
        {
            int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                continue;
            }

            return false;
        }

        if (sent <= 0)
        {
            return false;
        }

        totalSent += sent;
    }

    return true;
}

bool ServerBridge::SendPayload(const std::vector<std::uint8_t>& payload)
{
    if (!mConnected || mSocket == INVALID_SOCKET)
    {
        return false;
    }

    std::vector<std::uint8_t> framedPacket;
    framedPacket.reserve(kHeaderSize + payload.size());

    WriteLengthPrefix(framedPacket, static_cast<std::uint32_t>(payload.size()));
    framedPacket.insert(framedPacket.end(), payload.begin(), payload.end());

    return SendAll(framedPacket.data(), static_cast<int>(framedPacket.size()));
}

void ServerBridge::SendSignUpRequest(const std::string& playerName, const std::string& password)
{
    AuthRequest request{};
    request.playerName = playerName;
    request.password = password;
    SendPayload(WireFormat::MakeSignUpPacket(request));
}

void ServerBridge::SendSignInRequest(const std::string& playerName, const std::string& password)
{
    AuthRequest request{};
    request.playerName = playerName;
    request.password = password;
    SendPayload(WireFormat::MakeSignInPacket(request));
}

void ServerBridge::SendCreateRoomRequest(const CreateRoomRequest& request)
{
    SendPayload(WireFormat::MakeCreateRoomPacket(request));
}

void ServerBridge::SendJoinRoomRequest(int roomId)
{
    JoinRoomRequest request{};
    request.roomId = roomId;
    SendPayload(WireFormat::MakeJoinRoomPacket(request));
}


void ServerBridge::SendReadyToggleRequest(bool ready)
{
    SendPayload(WireFormat::MakeReadyTogglePacket(ready));
}

void ServerBridge::SendSubmitCellRequest(int row, int col, int value)
{
    CellInputRequest request{};
    request.row = row;
    request.col = col;
    request.value = value;
    SendPayload(WireFormat::MakeSubmitCellPacket(request));
}

void ServerBridge::SendPlayAgainRequest(bool playAgain)
{
    SendPayload(WireFormat::MakePlayAgainPacket(playAgain));
}

void ServerBridge::SendUseHintRequest(int row, int col)
{
    HintRequest request{};
    request.row = row;
    request.col = col;
    SendPayload(WireFormat::MakeUseHintPacket(request));
}

void ServerBridge::SendLeaveRoomRequest()
{
    SendPayload(WireFormat::MakeLeaveRoomRequestPacket());
}

void ServerBridge::SendHistoryRequest()
{
    SendPayload(WireFormat::MakeHistoryRequestPacket());
}

void ServerBridge::SendRequestPublicRoomList()
{
    SendPayload(WireFormat::MakeRequestPublicRoomListPacket());
}

void ServerBridge::SendRoomChatMessage(const std::string& text)
{
    SendPayload(WireFormat::MakeSendRoomChatPacket(text));
}

void ServerBridge::SendSearchUserRequest(const std::string& username)
{
    SendPayload(WireFormat::MakeSearchUserPacket(username));
}

void ServerBridge::SendFriendRequest(const std::string& username)
{
    FriendRequestPayload payload{};
    payload.targetUsername = username;
    SendPayload(WireFormat::MakeSendFriendRequestPacket(payload));
}

void ServerBridge::SendFriendRequestResponse(const std::string& fromUsername, bool accept)
{
    FriendRequestDecision decision{};
    decision.fromUsername = fromUsername;
    decision.accept = accept;
    SendPayload(WireFormat::MakeRespondFriendRequestPacket(decision));
}

void ServerBridge::SendRequestFriendsList()
{
    SendPayload(WireFormat::MakeRequestFriendsListPacket());
}

void ServerBridge::SendRequestPendingFriendRequests()
{
    SendPayload(WireFormat::MakeRequestPendingFriendRequestsPacket());
}

void ServerBridge::SendPrivateMessage(const std::string& targetUsername, const std::string& text)
{
    PrivateMessagePayload payload{};
    payload.targetUsername = targetUsername;
    payload.text = text;
    SendPayload(WireFormat::MakeSendPrivateMessagePacket(payload));
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

void ServerBridge::PushMessage(const BridgeMessage& msg)
{
    mInbox.push(msg);
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

        case PacketKind::CountdownStarted:
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

    std::uint8_t tempBuffer[kTempReadSize]{};

    while (true)
    {
        int bytesRead = ::recv(
            mSocket,
            reinterpret_cast<char*>(tempBuffer),
            kTempReadSize,
            0
        );

        if (bytesRead > 0)
        {
            mReceiveBuffer.insert(
                mReceiveBuffer.end(),
                tempBuffer,
                tempBuffer + bytesRead
            );

            while (mReceiveBuffer.size() >= kHeaderSize)
            {
                std::uint32_t payloadLength = ReadLengthPrefix(mReceiveBuffer, 0);

                if (mReceiveBuffer.size() < kHeaderSize + payloadLength)
                {
                    break;
                }

                std::vector<std::uint8_t> payload(
                    mReceiveBuffer.begin() + kHeaderSize,
                    mReceiveBuffer.begin() + kHeaderSize + payloadLength
                );

                DecodePacket(payload);

                mReceiveBuffer.erase(
                    mReceiveBuffer.begin(),
                    mReceiveBuffer.begin() + kHeaderSize + payloadLength
                );
            }
        }
        else if (bytesRead == 0)
        {
            Disconnect();
            break;
        }
        else
        {
            int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                break;
            }

            Disconnect();
            break;
        }
    }
}