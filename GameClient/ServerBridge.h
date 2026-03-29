#pragma once

#include <queue>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../Shared/PacketKinds.h"
#include "../Shared/SharedModels.h"

struct BridgeMessage
{
    PacketKind kind = PacketKind::Invalid;

    AuthResponseData authResponse;
    RoomJoinedResponseData roomJoin;
    LeaveRoomResponseData leaveRoomResponse;
    RoomSnapshot roomSnapshot;
    PuzzleData puzzleData;
    CellResult cellResult;
    CellClaimUpdate cellClaim;
    std::vector<PlayerScoreInfo> leaderboard;
    HintState hintState;
    std::vector<MatchHistoryRow> historyRows;
    std::vector<PublicRoomInfo> publicRooms;
    ChatMessage chatMessage;

    UserSearchResult userSearchResult;
    std::vector<FriendInfo> friendList;
    std::vector<FriendInfo> pendingFriendList;
    PrivateChatMessage privateChatMessage;

    std::string text;
    bool flag = false;
};

class ServerBridge
{
public:
    ServerBridge();
    ~ServerBridge();

    bool Connect(const std::string& host, unsigned short port);
    void Disconnect();
    bool IsConnected() const;

    void PollIncoming();

    void SendSignUpRequest(const std::string& playerName, const std::string& password);
    void SendSignInRequest(const std::string& playerName, const std::string& password);
    void SendCreateRoomRequest(const CreateRoomRequest& request);
    void SendJoinRoomRequest(int roomId);
    void SendReadyToggleRequest(bool ready);
    void SendSubmitCellRequest(int row, int col, int value);
    void SendPlayAgainRequest(bool playAgain);
    void SendUseHintRequest(int row, int col);
    void SendLeaveRoomRequest();

    void SendHistoryRequest();
    void SendRequestPublicRoomList();
    void SendRoomChatMessage(const std::string& text);

    void SendSearchUserRequest(const std::string& username);
    void SendFriendRequest(const std::string& username);
    void SendFriendRequestResponse(const std::string& fromUsername, bool accept);
    void SendRequestFriendsList();
    void SendRequestPendingFriendRequests();
    void SendPrivateMessage(const std::string& targetUsername, const std::string& text);

    bool HasPendingMessage() const;
    BridgeMessage PopNextMessage();

    int GetAssignedPlayerId() const;
    int GetAssignedRoomId() const;
    void ClearRoomAssignment();

private:
    bool InitializeWinsock();
    void ShutdownWinsock();
    bool SendPayload(const std::vector<std::uint8_t>& payload);
    bool SendAll(const std::uint8_t* data, int byteCount);
    bool SetSocketNonBlocking(SOCKET socketHandle);
    void DecodePacket(const std::vector<std::uint8_t>& payload);
    void PushMessage(const BridgeMessage& msg);

private:
    bool mWinsockInitialized = false;
    bool mConnected = false;
    SOCKET mSocket = INVALID_SOCKET;
    std::string mHost;
    unsigned short mPort = 0;

    int mAssignedPlayerId = 0;
    int mAssignedRoomId = 0;

    std::vector<std::uint8_t> mReceiveBuffer;
    std::queue<BridgeMessage> mInbox;
};