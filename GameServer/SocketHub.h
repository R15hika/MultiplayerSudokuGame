#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

class SocketHub
{
public:
    enum class EventType
    {
        Connected = 0,
        Disconnected,
        PacketReceived
    };

    struct Event
    {
        EventType type = EventType::Connected;
        int connectionId = 0;
        std::vector<std::uint8_t> payload;
    };

    SocketHub();
    ~SocketHub();

    bool Start(unsigned short port);
    void Stop();

    bool IsRunning() const;

    void PollEvents(std::vector<Event>& outEvents);

    bool SendPacket(int connectionId, const std::vector<std::uint8_t>& payload);
    void DisconnectClient(int connectionId);

    std::vector<int> GetConnectionIds() const;

private:
    struct ClientState
    {
        SOCKET socketHandle = INVALID_SOCKET;
        std::vector<std::uint8_t> receiveBuffer;
    };

private:
    bool InitializeWinsock();
    void ShutdownWinsock();

    bool SetSocketNonBlocking(SOCKET socketHandle);
    void AcceptPendingConnections(std::vector<Event>& outEvents);
    void ReceiveFromClients(std::vector<Event>& outEvents);

    bool SendAll(SOCKET socketHandle, const std::uint8_t* data, int byteCount);
    void RemoveClient(int connectionId);

private:
    bool mWinsockInitialized = false;
    bool mRunning = false;
    SOCKET mListenSocket = INVALID_SOCKET;
    int mNextConnectionId = 1;
    std::unordered_map<int, ClientState> mClients;
};