#pragma once

#include "../Shared/SharedModels.h"
#include "PlayerViewport.h"
#include "ServerBridge.h"

class WaitingRoom
{
public:
    void ApplyRoomSnapshot(const RoomSnapshot& snapshot);

    void ToggleReady(ServerBridge& bridge);
    bool IsLocalReady() const;

    bool HasEnoughPlayers() const;
    bool AreAllPlayersReady() const;

    const RoomSnapshot& GetSnapshot() const;

    void ProcessServerMessages(PlayerViewport& viewport, ServerBridge& bridge);

private:
    RoomSnapshot mSnapshot;
    bool mLocalReady = false;
};