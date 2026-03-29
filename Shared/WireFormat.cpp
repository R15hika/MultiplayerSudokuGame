#include "WireFormat.h"

void ByteWriter::WriteBool(bool value)
{
    mBuffer.push_back(value ? 1u : 0u);
}

void ByteWriter::WriteUInt8(std::uint8_t value)
{
    mBuffer.push_back(value);
}

void ByteWriter::WriteUInt16(std::uint16_t value)
{
    mBuffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    mBuffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void ByteWriter::WriteUInt32(std::uint32_t value)
{
    mBuffer.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    mBuffer.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    mBuffer.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    mBuffer.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void ByteWriter::WriteInt32(std::int32_t value)
{
    WriteUInt32(static_cast<std::uint32_t>(value));
}

void ByteWriter::WriteString(const std::string& value)
{
    WriteUInt32(static_cast<std::uint32_t>(value.size()));
    mBuffer.insert(mBuffer.end(), value.begin(), value.end());
}

const std::vector<std::uint8_t>& ByteWriter::GetBuffer() const
{
    return mBuffer;
}

ByteReader::ByteReader(const std::vector<std::uint8_t>& buffer)
    : mBuffer(buffer)
{
}

bool ByteReader::ReadBool()
{
    return ReadUInt8() != 0;
}

std::uint8_t ByteReader::ReadUInt8()
{
    RequireBytes(1);
    return mBuffer[mOffset++];
}

std::uint16_t ByteReader::ReadUInt16()
{
    RequireBytes(2);

    std::uint16_t value =
        (static_cast<std::uint16_t>(mBuffer[mOffset]) << 8) |
        (static_cast<std::uint16_t>(mBuffer[mOffset + 1]));

    mOffset += 2;
    return value;
}

std::uint32_t ByteReader::ReadUInt32()
{
    RequireBytes(4);

    std::uint32_t value =
        (static_cast<std::uint32_t>(mBuffer[mOffset]) << 24) |
        (static_cast<std::uint32_t>(mBuffer[mOffset + 1]) << 16) |
        (static_cast<std::uint32_t>(mBuffer[mOffset + 2]) << 8) |
        (static_cast<std::uint32_t>(mBuffer[mOffset + 3]));

    mOffset += 4;
    return value;
}

std::int32_t ByteReader::ReadInt32()
{
    return static_cast<std::int32_t>(ReadUInt32());
}

std::string ByteReader::ReadString()
{
    std::uint32_t size = ReadUInt32();
    RequireBytes(size);

    std::string value(
        reinterpret_cast<const char*>(mBuffer.data() + mOffset),
        size
    );

    mOffset += size;
    return value;
}

bool ByteReader::IsAtEnd() const
{
    return mOffset >= mBuffer.size();
}

void ByteReader::RequireBytes(std::size_t count)
{
    if (mOffset + count > mBuffer.size())
    {
        throw std::runtime_error("WireFormat buffer underflow");
    }
}

namespace WireFormat
{
    void WriteUserSearchResult(ByteWriter& writer, const UserSearchResult& value)
    {
        writer.WriteBool(value.found);
        writer.WriteString(value.username);
        writer.WriteString(value.text);
    }

    UserSearchResult ReadUserSearchResult(ByteReader& reader)
    {
        UserSearchResult value{};
        value.found = reader.ReadBool();
        value.username = reader.ReadString();
        value.text = reader.ReadString();
        return value;
    }

    void WriteFriendRequestPayload(ByteWriter& writer, const FriendRequestPayload& value)
    {
        writer.WriteString(value.targetUsername);
    }

    FriendRequestPayload ReadFriendRequestPayload(ByteReader& reader)
    {
        FriendRequestPayload value{};
        value.targetUsername = reader.ReadString();
        return value;
    }

    void WriteFriendRequestDecision(ByteWriter& writer, const FriendRequestDecision& value)
    {
        writer.WriteString(value.fromUsername);
        writer.WriteBool(value.accept);
    }

    FriendRequestDecision ReadFriendRequestDecision(ByteReader& reader)
    {
        FriendRequestDecision value{};
        value.fromUsername = reader.ReadString();
        value.accept = reader.ReadBool();
        return value;
    }

    void WriteFriendInfo(ByteWriter& writer, const FriendInfo& value)
    {
        writer.WriteString(value.username);
    }

    FriendInfo ReadFriendInfo(ByteReader& reader)
    {
        FriendInfo value{};
        value.username = reader.ReadString();
        return value;
    }

    void WriteFriendList(ByteWriter& writer, const std::vector<FriendInfo>& value)
    {
        writer.WriteUInt32(static_cast<std::uint32_t>(value.size()));
        for (const FriendInfo& item : value)
        {
            WriteFriendInfo(writer, item);
        }
    }

    std::vector<FriendInfo> ReadFriendList(ByteReader& reader)
    {
        std::vector<FriendInfo> value;
        std::uint32_t count = reader.ReadUInt32();
        value.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
        {
            value.push_back(ReadFriendInfo(reader));
        }

        return value;
    }

    void WritePrivateMessagePayload(ByteWriter& writer, const PrivateMessagePayload& value)
    {
        writer.WriteString(value.targetUsername);
        writer.WriteString(value.text);
    }

    PrivateMessagePayload ReadPrivateMessagePayload(ByteReader& reader)
    {
        PrivateMessagePayload value{};
        value.targetUsername = reader.ReadString();
        value.text = reader.ReadString();
        return value;
    }

    void WritePrivateChatMessage(ByteWriter& writer, const PrivateChatMessage& value)
    {
        writer.WriteString(value.senderName);
        writer.WriteString(value.receiverName);
        writer.WriteString(value.text);
        writer.WriteString(value.timestamp);
    }

    PrivateChatMessage ReadPrivateChatMessage(ByteReader& reader)
    {
        PrivateChatMessage value{};
        value.senderName = reader.ReadString();
        value.receiverName = reader.ReadString();
        value.text = reader.ReadString();
        value.timestamp = reader.ReadString();
        return value;
    }


    std::vector<std::uint8_t> WritePacketKind(PacketKind kind)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(kind));
        return writer.GetBuffer();
    }

    void WriteAuthRequest(ByteWriter& writer, const AuthRequest& value)
    {
        writer.WriteString(value.playerName);
        writer.WriteString(value.password);
    }

    AuthRequest ReadAuthRequest(ByteReader& reader)
    {
        AuthRequest value{};
        value.playerName = reader.ReadString();
        value.password = reader.ReadString();
        return value;
    }

    void WriteAuthResponse(ByteWriter& writer, const AuthResponseData& value)
    {
        writer.WriteBool(value.success);
        writer.WriteInt32(value.playerId);
        writer.WriteString(value.text);
    }

    AuthResponseData ReadAuthResponse(ByteReader& reader)
    {
        AuthResponseData value{};
        value.success = reader.ReadBool();
        value.playerId = reader.ReadInt32();
        value.text = reader.ReadString();
        return value;
    }

    PacketKind ReadPacketKind(ByteReader& reader)
    {
        return static_cast<PacketKind>(reader.ReadUInt16());
    }

    void WriteCreateRoomRequest(ByteWriter& writer, const CreateRoomRequest& value)
    {
        writer.WriteInt32(value.maxPlayers);
        writer.WriteUInt8(static_cast<std::uint8_t>(value.visibility));
    }

    CreateRoomRequest ReadCreateRoomRequest(ByteReader& reader)
    {
        CreateRoomRequest value{};
        value.maxPlayers = reader.ReadInt32();
        value.visibility = static_cast<RoomVisibility>(reader.ReadUInt8());
        return value;
    }

    void WriteJoinRoomRequest(ByteWriter& writer, const JoinRoomRequest& value)
    {
        writer.WriteInt32(value.roomId);
    }

    JoinRoomRequest ReadJoinRoomRequest(ByteReader& reader)
    {
        JoinRoomRequest value{};
        value.roomId = reader.ReadInt32();
        return value;
    }


    void WriteRoomJoinedResponse(ByteWriter& writer, const RoomJoinedResponseData& value)
    {
        writer.WriteBool(value.success);
        writer.WriteInt32(value.playerId);
        writer.WriteInt32(value.roomId);
        writer.WriteString(value.text);
    }

    RoomJoinedResponseData ReadRoomJoinedResponse(ByteReader& reader)
    {
        RoomJoinedResponseData value{};
        value.success = reader.ReadBool();
        value.playerId = reader.ReadInt32();
        value.roomId = reader.ReadInt32();
        value.text = reader.ReadString();
        return value;
    }

    void WriteLeaveRoomResponse(ByteWriter& writer, const LeaveRoomResponseData& value)
    {
        writer.WriteBool(value.success);
        writer.WriteString(value.text);
    }

    LeaveRoomResponseData ReadLeaveRoomResponse(ByteReader& reader)
    {
        LeaveRoomResponseData value{};
        value.success = reader.ReadBool();
        value.text = reader.ReadString();
        return value;
    }

    void WriteCellInputRequest(ByteWriter& writer, const CellInputRequest& value)
    {
        writer.WriteInt32(value.row);
        writer.WriteInt32(value.col);
        writer.WriteInt32(value.value);
    }

    CellInputRequest ReadCellInputRequest(ByteReader& reader)
    {
        CellInputRequest value{};
        value.row = reader.ReadInt32();
        value.col = reader.ReadInt32();
        value.value = reader.ReadInt32();
        return value;
    }

    void WriteCellResult(ByteWriter& writer, const CellResult& value)
    {
        writer.WriteInt32(value.playerId);
        writer.WriteInt32(value.row);
        writer.WriteInt32(value.col);
        writer.WriteInt32(value.submittedValue);
        writer.WriteBool(value.correct);
        writer.WriteBool(value.awarded);
        writer.WriteInt32(value.pointsAwarded);
        writer.WriteUInt8(static_cast<std::uint8_t>(value.firstSolver));
    }

    CellResult ReadCellResult(ByteReader& reader)
    {
        CellResult value{};
        value.playerId = reader.ReadInt32();
        value.row = reader.ReadInt32();
        value.col = reader.ReadInt32();
        value.submittedValue = reader.ReadInt32();
        value.correct = reader.ReadBool();
        value.awarded = reader.ReadBool();
        value.pointsAwarded = reader.ReadInt32();
        value.firstSolver = static_cast<CellOwner>(reader.ReadUInt8());
        return value;
    }

    void WriteCellClaimUpdate(ByteWriter& writer, const CellClaimUpdate& value)
    {
        writer.WriteInt32(value.row);
        writer.WriteInt32(value.col);
        writer.WriteUInt8(static_cast<std::uint8_t>(value.firstSolver));
    }

    CellClaimUpdate ReadCellClaimUpdate(ByteReader& reader)
    {
        CellClaimUpdate value{};
        value.row = reader.ReadInt32();
        value.col = reader.ReadInt32();
        value.firstSolver = static_cast<CellOwner>(reader.ReadUInt8());
        return value;
    }

    void WriteHintState(ByteWriter& writer, const HintState& value)
    {
        writer.WriteBool(value.unlocked);
        writer.WriteInt32(value.availableCount);
    }

    HintState ReadHintState(ByteReader& reader)
    {
        HintState value{};
        value.unlocked = reader.ReadBool();
        value.availableCount = reader.ReadInt32();
        return value;
    }

    void WriteMatchHistoryRow(ByteWriter& writer, const MatchHistoryRow& value)
    {
        writer.WriteString(value.playedAt);
        writer.WriteString(value.result);
        writer.WriteInt32(value.finalScore);
        writer.WriteInt32(value.wrongAttempts);
        writer.WriteInt32(value.hintsUsed);
        writer.WriteInt32(value.totalTimeSeconds);
        writer.WriteString(std::to_string(value.averageMoveSeconds));
        writer.WriteInt32(value.finalRank);
        writer.WriteInt32(value.totalPlayers);
    }

    MatchHistoryRow ReadMatchHistoryRow(ByteReader& reader)
    {
        MatchHistoryRow value{};
        value.playedAt = reader.ReadString();
        value.result = reader.ReadString();
        value.finalScore = reader.ReadInt32();
        value.wrongAttempts = reader.ReadInt32();
        value.hintsUsed = reader.ReadInt32();
        value.totalTimeSeconds = reader.ReadInt32();
        value.averageMoveSeconds = std::stod(reader.ReadString());
        value.finalRank = reader.ReadInt32();
        value.totalPlayers = reader.ReadInt32();
        return value;
    }

    void WritePublicRoomInfo(ByteWriter& writer, const PublicRoomInfo& value)
    {
        writer.WriteInt32(value.roomId);
        writer.WriteInt32(value.currentPlayers);
        writer.WriteInt32(value.maxPlayers);
        writer.WriteString(value.hostName);
    }

    PublicRoomInfo ReadPublicRoomInfo(ByteReader& reader)
    {
        PublicRoomInfo value{};
        value.roomId = reader.ReadInt32();
        value.currentPlayers = reader.ReadInt32();
        value.maxPlayers = reader.ReadInt32();
        value.hostName = reader.ReadString();
        return value;
    }

    void WritePublicRoomList(ByteWriter& writer, const std::vector<PublicRoomInfo>& value)
    {
        writer.WriteUInt32(static_cast<std::uint32_t>(value.size()));
        for (const PublicRoomInfo& room : value)
        {
            WritePublicRoomInfo(writer, room);
        }
    }

    std::vector<PublicRoomInfo> ReadPublicRoomList(ByteReader& reader)
    {
        std::vector<PublicRoomInfo> value;
        std::uint32_t count = reader.ReadUInt32();
        value.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
        {
            value.push_back(ReadPublicRoomInfo(reader));
        }

        return value;
    }

    void WriteChatMessage(ByteWriter& writer, const ChatMessage& value)
    {
        writer.WriteString(value.senderName);
        writer.WriteString(value.text);
        writer.WriteString(value.timestamp);
    }

    ChatMessage ReadChatMessage(ByteReader& reader)
    {
        ChatMessage value{};
        value.senderName = reader.ReadString();
        value.text = reader.ReadString();
        value.timestamp = reader.ReadString();
        return value;
    }

    void WriteMatchHistoryList(ByteWriter& writer, const std::vector<MatchHistoryRow>& value)
    {
        writer.WriteUInt32(static_cast<std::uint32_t>(value.size()));
        for (const MatchHistoryRow& row : value)
        {
            WriteMatchHistoryRow(writer, row);
        }
    }

    std::vector<MatchHistoryRow> ReadMatchHistoryList(ByteReader& reader)
    {
        std::vector<MatchHistoryRow> value;
        std::uint32_t count = reader.ReadUInt32();
        value.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
        {
            value.push_back(ReadMatchHistoryRow(reader));
        }

        return value;
    }

    void WriteHintRequest(ByteWriter& writer, const HintRequest& value)
    {
        writer.WriteInt32(value.row);
        writer.WriteInt32(value.col);
    }

    HintRequest ReadHintRequest(ByteReader& reader)
    {
        HintRequest value{};
        value.row = reader.ReadInt32();
        value.col = reader.ReadInt32();
        return value;
    }

    void WritePlayerScoreInfo(ByteWriter& writer, const PlayerScoreInfo& value)
    {
        writer.WriteInt32(value.playerId);
        writer.WriteString(value.playerName);
        writer.WriteInt32(value.score);
        writer.WriteInt32(value.rank);
        writer.WriteBool(value.connected);
        writer.WriteBool(value.ready);
        writer.WriteBool(value.finished);
        writer.WriteBool(value.hintUnlocked);
        writer.WriteInt32(value.currentStreak);
        writer.WriteUInt8(static_cast<std::uint8_t>(value.displayColor));
    }

    PlayerScoreInfo ReadPlayerScoreInfo(ByteReader& reader)
    {
        PlayerScoreInfo value{};
        value.playerId = reader.ReadInt32();
        value.playerName = reader.ReadString();
        value.score = reader.ReadInt32();
        value.rank = reader.ReadInt32();
        value.connected = reader.ReadBool();
        value.ready = reader.ReadBool();
        value.finished = reader.ReadBool();
        value.hintUnlocked = reader.ReadBool();
        value.currentStreak = reader.ReadInt32();
        value.displayColor = static_cast<CellOwner>(reader.ReadUInt8());
        return value;
    }

    void WritePlayerScoreList(ByteWriter& writer, const std::vector<PlayerScoreInfo>& value)
    {
        writer.WriteUInt32(static_cast<std::uint32_t>(value.size()));
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            WritePlayerScoreInfo(writer, value[i]);
        }
    }

    std::vector<PlayerScoreInfo> ReadPlayerScoreList(ByteReader& reader)
    {
        std::vector<PlayerScoreInfo> value;
        std::uint32_t count = reader.ReadUInt32();
        value.reserve(count);

        for (std::uint32_t i = 0; i < count; ++i)
        {
            value.push_back(ReadPlayerScoreInfo(reader));
        }

        return value;
    }

    void WriteRoomSnapshot(ByteWriter& writer, const RoomSnapshot& value)
    {
        writer.WriteInt32(value.config.maxPlayers);
        writer.WriteBool(value.countdownStarted);
        WritePlayerScoreList(writer, value.players);
    }

    RoomSnapshot ReadRoomSnapshot(ByteReader& reader)
    {
        RoomSnapshot value{};
        value.config.maxPlayers = reader.ReadInt32();
        value.countdownStarted = reader.ReadBool();
        value.players = ReadPlayerScoreList(reader);
        return value;
    }

    void WritePuzzleData(ByteWriter& writer, const PuzzleData& value)
    {
        for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
        {
            for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
            {
                writer.WriteInt32(value.puzzle[row][col]);
            }
        }

        for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
        {
            for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
            {
                writer.WriteInt32(value.solution[row][col]);
            }
        }
    }

    PuzzleData ReadPuzzleData(ByteReader& reader)
    {
        PuzzleData value{};

        for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
        {
            for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
            {
                value.puzzle[row][col] = reader.ReadInt32();
            }
        }

        for (int row = 0; row < RuleBook::BOARD_SIZE; ++row)
        {
            for (int col = 0; col < RuleBook::BOARD_SIZE; ++col)
            {
                value.solution[row][col] = reader.ReadInt32();
            }
        }

        return value;
    }

    std::vector<std::uint8_t> MakeSignUpPacket(const AuthRequest& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SignUpRequest));
        WriteAuthRequest(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSignInPacket(const AuthRequest& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SignInRequest));
        WriteAuthRequest(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeAuthResponsePacket(const AuthResponseData& response)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::AuthResponse));
        WriteAuthResponse(writer, response);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeCreateRoomPacket(const CreateRoomRequest& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::CreateRoomRequest));
        WriteCreateRoomRequest(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeJoinRoomPacket(const JoinRoomRequest& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::JoinRoomRequest));
        WriteJoinRoomRequest(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeLeaveRoomRequestPacket()
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::LeaveRoomRequest));
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeReadyTogglePacket(bool ready)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::ReadyToggleRequest));
        writer.WriteBool(ready);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSubmitCellPacket(const CellInputRequest& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SubmitCellRequest));
        WriteCellInputRequest(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakePlayAgainPacket(bool playAgain)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::TogglePlayAgainRequest));
        writer.WriteBool(playAgain);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeUseHintPacket(const HintRequest& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::UseHintRequest));
        WriteHintRequest(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSearchUserPacket(const std::string& username)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SearchUserRequest));
        writer.WriteString(username);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSearchUserResponsePacket(const UserSearchResult& result)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SearchUserResponse));
        WriteUserSearchResult(writer, result);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSendFriendRequestPacket(const FriendRequestPayload& request)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SendFriendRequest));
        WriteFriendRequestPayload(writer, request);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRespondFriendRequestPacket(const FriendRequestDecision& decision)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RespondFriendRequest));
        WriteFriendRequestDecision(writer, decision);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRequestFriendsListPacket()
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RequestFriendsList));
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRequestPendingFriendRequestsPacket()
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RequestPendingFriendRequests));
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeFriendsListResponsePacket(const std::vector<FriendInfo>& friends)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::FriendsListResponse));
        WriteFriendList(writer, friends);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakePendingFriendRequestsResponsePacket(const std::vector<FriendInfo>& requests)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::PendingFriendRequestsResponse));
        WriteFriendList(writer, requests);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSendPrivateMessagePacket(const PrivateMessagePayload& payload)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SendPrivateMessage));
        WritePrivateMessagePayload(writer, payload);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakePrivateMessageReceivedPacket(const PrivateChatMessage& msg)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::PrivateMessageReceived));
        WritePrivateChatMessage(writer, msg);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeFriendRequestReceivedPacket(const FriendInfo& fromUser)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::FriendRequestReceived));
        WriteFriendInfo(writer, fromUser);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeFriendRequestResponsePacket(const std::string& text)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::FriendRequestResponse));
        writer.WriteString(text);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRequestPublicRoomListPacket()
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RequestPublicRoomList));
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeSendRoomChatPacket(const std::string& text)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::SendRoomChatMessage));
        writer.WriteString(text);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeHistoryRequestPacket()
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::HistoryRequest));
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRoomJoinedResponsePacket(const RoomJoinedResponseData& response)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RoomJoinedResponse));
        WriteRoomJoinedResponse(writer, response);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeLeaveRoomResponsePacket(const LeaveRoomResponseData& response)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::LeaveRoomResponse));
        WriteLeaveRoomResponse(writer, response);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRoomClosedNoticePacket(const std::string& text)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RoomClosedNotice));
        writer.WriteString(text);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeCellCheckedPacket(const CellResult& result)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::CellCheckedResponse));
        WriteCellResult(writer, result);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeCellClaimUpdatePacket(const CellClaimUpdate& update)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::CellClaimUpdateNotice));
        WriteCellClaimUpdate(writer, update);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeWaitingRoomPacket(const RoomSnapshot& snapshot)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::WaitingRoomUpdate));
        WriteRoomSnapshot(writer, snapshot);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeMatchStartedPacket(const PuzzleData& puzzle)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::MatchStarted));
        WritePuzzleData(writer, puzzle);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeLeaderboardPacket(const std::vector<PlayerScoreInfo>& leaderboard)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::LeaderboardUpdate));
        WritePlayerScoreList(writer, leaderboard);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeHintUnlockedPacket(const HintState& hintState)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::HintUnlockedNotice));
        WriteHintState(writer, hintState);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakePublicRoomListResponsePacket(const std::vector<PublicRoomInfo>& rooms)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::PublicRoomListResponse));
        WritePublicRoomList(writer, rooms);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRoomChatMessagePacket(const ChatMessage& msg)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RoomChatMessageReceived));
        WriteChatMessage(writer, msg);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeHistoryResponsePacket(const std::vector<MatchHistoryRow>& history)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::HistoryResponse));
        WriteMatchHistoryList(writer, history);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeRoundFinishedPacket(const std::string& text)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::RoundFinishedNotice));
        writer.WriteString(text);
        return writer.GetBuffer();
    }

    std::vector<std::uint8_t> MakeErrorPacket(const std::string& text)
    {
        ByteWriter writer;
        writer.WriteUInt16(static_cast<std::uint16_t>(PacketKind::ErrorNotice));
        writer.WriteString(text);
        return writer.GetBuffer();
    }
}