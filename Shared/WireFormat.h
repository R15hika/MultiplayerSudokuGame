#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "PacketKinds.h"
#include "SharedModels.h"

class ByteWriter
{
public:
    void WriteBool(bool value);
    void WriteUInt8(std::uint8_t value);
    void WriteUInt16(std::uint16_t value);
    void WriteUInt32(std::uint32_t value);
    void WriteInt32(std::int32_t value);
    void WriteString(const std::string& value);

    const std::vector<std::uint8_t>& GetBuffer() const;

private:
    std::vector<std::uint8_t> mBuffer;
};

class ByteReader
{
public:
    explicit ByteReader(const std::vector<std::uint8_t>& buffer);

    bool ReadBool();
    std::uint8_t ReadUInt8();
    std::uint16_t ReadUInt16();
    std::uint32_t ReadUInt32();
    std::int32_t ReadInt32();
    std::string ReadString();

    bool IsAtEnd() const;

private:
    void RequireBytes(std::size_t count);

private:
    const std::vector<std::uint8_t>& mBuffer;
    std::size_t mOffset = 0;
};

namespace WireFormat
{
    void WriteUserSearchResult(ByteWriter& writer, const UserSearchResult& value);
    UserSearchResult ReadUserSearchResult(ByteReader& reader);

    void WriteFriendRequestPayload(ByteWriter& writer, const FriendRequestPayload& value);
    FriendRequestPayload ReadFriendRequestPayload(ByteReader& reader);

    void WriteFriendRequestDecision(ByteWriter& writer, const FriendRequestDecision& value);
    FriendRequestDecision ReadFriendRequestDecision(ByteReader& reader);

    void WriteFriendInfo(ByteWriter& writer, const FriendInfo& value);
    FriendInfo ReadFriendInfo(ByteReader& reader);

    void WriteFriendList(ByteWriter& writer, const std::vector<FriendInfo>& value);
    std::vector<FriendInfo> ReadFriendList(ByteReader& reader);

    void WritePrivateMessagePayload(ByteWriter& writer, const PrivateMessagePayload& value);
    PrivateMessagePayload ReadPrivateMessagePayload(ByteReader& reader);

    void WritePrivateChatMessage(ByteWriter& writer, const PrivateChatMessage& value);
    PrivateChatMessage ReadPrivateChatMessage(ByteReader& reader);

    void WriteChatMessage(ByteWriter& writer, const ChatMessage& value);
    ChatMessage ReadChatMessage(ByteReader& reader);

    void WritePublicRoomList(ByteWriter& writer, const std::vector<PublicRoomInfo>& value);
    std::vector<PublicRoomInfo> ReadPublicRoomList(ByteReader& reader);

    std::vector<std::uint8_t> MakeRequestPublicRoomListPacket();
    std::vector<std::uint8_t> MakePublicRoomListResponsePacket(const std::vector<PublicRoomInfo>& rooms);

    std::vector<std::uint8_t> MakeSearchUserPacket(const std::string& username);
    std::vector<std::uint8_t> MakeSearchUserResponsePacket(const UserSearchResult& result);

    std::vector<std::uint8_t> MakeSendFriendRequestPacket(const FriendRequestPayload& request);
    std::vector<std::uint8_t> MakeRespondFriendRequestPacket(const FriendRequestDecision& decision);
    std::vector<std::uint8_t> MakeRequestFriendsListPacket();
    std::vector<std::uint8_t> MakeRequestPendingFriendRequestsPacket();

    std::vector<std::uint8_t> MakeFriendsListResponsePacket(const std::vector<FriendInfo>& friends);
    std::vector<std::uint8_t> MakePendingFriendRequestsResponsePacket(const std::vector<FriendInfo>& requests);

    std::vector<std::uint8_t> MakeSendPrivateMessagePacket(const PrivateMessagePayload& payload);
    std::vector<std::uint8_t> MakePrivateMessageReceivedPacket(const PrivateChatMessage& msg);
    std::vector<std::uint8_t> MakeFriendRequestReceivedPacket(const FriendInfo& fromUser);
    std::vector<std::uint8_t> MakeFriendRequestResponsePacket(const std::string& text);
    
    std::vector<std::uint8_t> WritePacketKind(PacketKind kind);
    PacketKind ReadPacketKind(ByteReader& reader);

    void WriteAuthRequest(ByteWriter& writer, const AuthRequest& value);
    AuthRequest ReadAuthRequest(ByteReader& reader);

    void WriteAuthResponse(ByteWriter& writer, const AuthResponseData& value);
    AuthResponseData ReadAuthResponse(ByteReader& reader);

    void WriteCreateRoomRequest(ByteWriter& writer, const CreateRoomRequest& value);
    CreateRoomRequest ReadCreateRoomRequest(ByteReader& reader);

    void WriteJoinRoomRequest(ByteWriter& writer, const JoinRoomRequest& value);
    JoinRoomRequest ReadJoinRoomRequest(ByteReader& reader);

    void WriteRoomJoinedResponse(ByteWriter& writer, const RoomJoinedResponseData& value);
    RoomJoinedResponseData ReadRoomJoinedResponse(ByteReader& reader);

    void WriteLeaveRoomResponse(ByteWriter& writer, const LeaveRoomResponseData& value);
    LeaveRoomResponseData ReadLeaveRoomResponse(ByteReader& reader);

    void WriteCellInputRequest(ByteWriter& writer, const CellInputRequest& value);
    CellInputRequest ReadCellInputRequest(ByteReader& reader);

    void WriteCellResult(ByteWriter& writer, const CellResult& value);
    CellResult ReadCellResult(ByteReader& reader);

    void WriteCellClaimUpdate(ByteWriter& writer, const CellClaimUpdate& value);
    CellClaimUpdate ReadCellClaimUpdate(ByteReader& reader);

    void WriteHintState(ByteWriter& writer, const HintState& value);
    HintState ReadHintState(ByteReader& reader);

    void WriteMatchHistoryRow(ByteWriter& writer, const MatchHistoryRow& value);
    MatchHistoryRow ReadMatchHistoryRow(ByteReader& reader);

    void WritePublicRoomInfo(ByteWriter& writer, const PublicRoomInfo& value);
    PublicRoomInfo ReadPublicRoomInfo(ByteReader& reader);

    void WritePublicRoomList(ByteWriter& writer, const std::vector<PublicRoomInfo>& value);
    std::vector<PublicRoomInfo> ReadPublicRoomList(ByteReader& reader);

    void WriteMatchHistoryList(ByteWriter& writer, const std::vector<MatchHistoryRow>& value);
    std::vector<MatchHistoryRow> ReadMatchHistoryList(ByteReader& reader);


    void WriteHintRequest(ByteWriter& writer, const HintRequest& value);
    HintRequest ReadHintRequest(ByteReader& reader);

    void WritePlayerScoreInfo(ByteWriter& writer, const PlayerScoreInfo& value);
    PlayerScoreInfo ReadPlayerScoreInfo(ByteReader& reader);

    void WritePlayerScoreList(ByteWriter& writer, const std::vector<PlayerScoreInfo>& value);
    std::vector<PlayerScoreInfo> ReadPlayerScoreList(ByteReader& reader);

    void WriteRoomSnapshot(ByteWriter& writer, const RoomSnapshot& value);
    RoomSnapshot ReadRoomSnapshot(ByteReader& reader);

    void WritePuzzleData(ByteWriter& writer, const PuzzleData& value);
    PuzzleData ReadPuzzleData(ByteReader& reader);

    std::vector<std::uint8_t> MakeSignUpPacket(const AuthRequest& request);
    std::vector<std::uint8_t> MakeSignInPacket(const AuthRequest& request);
    std::vector<std::uint8_t> MakeAuthResponsePacket(const AuthResponseData& response);
    std::vector<std::uint8_t> MakeRoomChatMessagePacket(const ChatMessage& msg);
    std::vector<std::uint8_t> MakeSendRoomChatPacket(const std::string& text);
    std::vector<std::uint8_t> MakeCreateRoomPacket(const CreateRoomRequest& request);
    std::vector<std::uint8_t> MakeJoinRoomPacket(const JoinRoomRequest& request);
    std::vector<std::uint8_t> MakeLeaveRoomRequestPacket();
    std::vector<std::uint8_t> MakeReadyTogglePacket(bool ready);
    std::vector<std::uint8_t> MakeSubmitCellPacket(const CellInputRequest& request);
    std::vector<std::uint8_t> MakePlayAgainPacket(bool playAgain);
    std::vector<std::uint8_t> MakeUseHintPacket(const HintRequest& request);
    std::vector<std::uint8_t> MakeHistoryRequestPacket();
    std::vector<std::uint8_t> MakeRequestPublicRoomListPacket();

    std::vector<std::uint8_t> MakeRoomJoinedResponsePacket(const RoomJoinedResponseData& response);
    std::vector<std::uint8_t> MakeLeaveRoomResponsePacket(const LeaveRoomResponseData& response);
    std::vector<std::uint8_t> MakeRoomClosedNoticePacket(const std::string& text);
    std::vector<std::uint8_t> MakeCellCheckedPacket(const CellResult& result);
    std::vector<std::uint8_t> MakeCellClaimUpdatePacket(const CellClaimUpdate& update);
    std::vector<std::uint8_t> MakeWaitingRoomPacket(const RoomSnapshot& snapshot);
    std::vector<std::uint8_t> MakeMatchStartedPacket(const PuzzleData& puzzle);
    std::vector<std::uint8_t> MakeLeaderboardPacket(const std::vector<PlayerScoreInfo>& leaderboard);
    std::vector<std::uint8_t> MakeHintUnlockedPacket(const HintState& hintState);
    std::vector<std::uint8_t> MakeRoundFinishedPacket(const std::string& text);
    std::vector<std::uint8_t> MakeHistoryResponsePacket(const std::vector<MatchHistoryRow>& history);
    std::vector<std::uint8_t> MakePublicRoomListResponsePacket(const std::vector<PublicRoomInfo>& rooms);
    std::vector<std::uint8_t> MakeErrorPacket(const std::string& text);
}