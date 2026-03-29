#pragma once

#include <cstdint>

enum class PacketKind : std::uint16_t
{
    Invalid = 0,

    // Client -> Server
    SignUpRequest,
    SignInRequest,
    CreateRoomRequest,
    JoinRoomRequest,
    LeaveRoomRequest,
    ReadyToggleRequest,
    SubmitCellRequest,
    TogglePlayAgainRequest,
    UseHintRequest,
    HistoryRequest,
    RequestPublicRoomList,
    SearchUserRequest,

    SendGlobalChatMessage,
    SendRoomChatMessage,
    SendPrivateMessage,
    SendFriendRequest,
    RespondFriendRequest,
    RequestFriendsList,
    RequestPendingFriendRequests,


    // Server -> Client
    AuthResponse,
    RoomJoinedResponse,
    LeaveRoomResponse,
    RoomClosedNotice,
    WaitingRoomUpdate,
    CountdownStarted,
    MatchStarted,
    CellCheckedResponse,
    CellClaimUpdateNotice,
    MatchStateUpdate,
    LeaderboardUpdate,
    HintUnlockedNotice,
    HistoryResponse,
    RoundFinishedNotice,
    PublicRoomListResponse,
    SearchUserResponse,
    ErrorNotice,

    GlobalChatMessageReceived,
    RoomChatMessageReceived,
    PrivateMessageReceived,
    FriendRequestReceived,
    FriendRequestResponse,
    FriendsListResponse,
    PendingFriendRequestsResponse
};