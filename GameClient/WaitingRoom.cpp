#include "WaitingRoom.h"

void WaitingRoom::ApplyRoomSnapshot(const RoomSnapshot& snapshot)
{
    mSnapshot = snapshot;

    bool foundReady = false;
    for (const PlayerScoreInfo& player : mSnapshot.players)
    {
        if (player.ready)
        {
            // temporary fallback; local state should really be keyed by local player id
        }
    }
}

void WaitingRoom::ToggleReady(ServerBridge& bridge)
{
    if (!bridge.IsConnected())
    {
        return;
    }

    mLocalReady = !mLocalReady;
    bridge.SendReadyToggleRequest(mLocalReady);
}

bool WaitingRoom::IsLocalReady() const
{
    return mLocalReady;
}

bool WaitingRoom::HasEnoughPlayers() const
{
    return static_cast<int>(mSnapshot.players.size()) >= RuleBook::MIN_PLAYERS;
}

bool WaitingRoom::AreAllPlayersReady() const
{
    if (mSnapshot.players.empty())
    {
        return false;
    }

    if (static_cast<int>(mSnapshot.players.size()) < mSnapshot.config.maxPlayers)
    {
        return false;
    }

    for (const PlayerScoreInfo& player : mSnapshot.players)
    {
        if (!player.connected || !player.ready)
        {
            return false;
        }
    }

    return true;
}

const RoomSnapshot& WaitingRoom::GetSnapshot() const
{
    return mSnapshot;
}

void WaitingRoom::ProcessServerMessages(PlayerViewport& viewport, ServerBridge& bridge)
{
    while (bridge.HasPendingMessage())
    {
        BridgeMessage msg = bridge.PopNextMessage();

        switch (msg.kind)
        {
        case PacketKind::WaitingRoomUpdate:
            ApplyRoomSnapshot(msg.roomSnapshot);
            viewport.SetPhase(ScreenPhase::WaitingRoom);
            break;

        case PacketKind::CountdownStarted:
            viewport.SetPhase(ScreenPhase::Countdown);
            break;

        case PacketKind::MatchStarted:
            viewport.ResetForNewMatch();
            viewport.ApplyPuzzle(msg.puzzleData);
            viewport.SetPhase(ScreenPhase::Playing);
            break;

        default:
            break;
        }
    }
}