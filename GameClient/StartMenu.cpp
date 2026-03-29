#include "StartMenu.h"

void StartMenu::SetPlayerName(const std::string& name)
{
    if (!name.empty())
    {
        mPlayerName = name;
    }
}

void StartMenu::SetPassword(const std::string& password)
{
    mPassword = password;
}

const std::string& StartMenu::GetPassword() const
{
    return mPassword;
}

void StartMenu::SignUp(ServerBridge& bridge)
{
    if (!bridge.IsConnected())
    {
        return;
    }

    bridge.SendSignUpRequest(mPlayerName, mPassword);
}

void StartMenu::SignIn(ServerBridge& bridge)
{
    if (!bridge.IsConnected())
    {
        return;
    }

    bridge.SendSignInRequest(mPlayerName, mPassword);
}

void StartMenu::SetDesiredPlayerCount(int count)
{
    if (count < RuleBook::MIN_PLAYERS)
    {
        mDesiredPlayerCount = RuleBook::MIN_PLAYERS;
    }
    else if (count > RuleBook::MAX_PLAYERS)
    {
        mDesiredPlayerCount = RuleBook::MAX_PLAYERS;
    }
    else
    {
        mDesiredPlayerCount = count;
    }
}

int StartMenu::GetDesiredPlayerCount() const
{
    return mDesiredPlayerCount;
}

void StartMenu::SetTargetRoomId(int roomId)
{
    mTargetRoomId = roomId;
}

int StartMenu::GetTargetRoomId() const
{
    return mTargetRoomId;
}

void StartMenu::SetRoomVisibility(RoomVisibility visibility)
{
    mRoomVisibility = visibility;
}

RoomVisibility StartMenu::GetRoomVisibility() const
{
    return mRoomVisibility;
}

void StartMenu::CreateRoom(PlayerViewport& viewport, ServerBridge& bridge)
{
    UNREFERENCED_PARAMETER(viewport);

    if (!bridge.IsConnected())
    {
        return;
    }

    CreateRoomRequest request{};
    request.maxPlayers = mDesiredPlayerCount;
    request.visibility = mRoomVisibility;

    bridge.SendCreateRoomRequest(request);
}

void StartMenu::JoinRoom(PlayerViewport& viewport, ServerBridge& bridge)
{
    UNREFERENCED_PARAMETER(viewport);

    if (!bridge.IsConnected())
    {
        return;
    }

    bridge.SendJoinRoomRequest(mTargetRoomId);
}