#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

struct NetAddress
{
    sockaddr_in addr{};

    bool operator==(const NetAddress& other) const
    {
        return addr.sin_family == other.addr.sin_family &&
            addr.sin_port == other.addr.sin_port &&
            addr.sin_addr.s_addr == other.addr.sin_addr.s_addr;
    }
};

struct NetAddressHash
{
    std::size_t operator()(const NetAddress& value) const
    {
        std::size_t h1 = std::hash<unsigned long>{}(value.addr.sin_addr.s_addr);
        std::size_t h2 = std::hash<unsigned short>{}(value.addr.sin_port);
        return h1 ^ (h2 << 1);
    }
};

class SocketHub
{
public:
    struct Event
    {
        NetAddress from{};
        std::vector<std::uint8_t> payload;
    };

    SocketHub();
    ~SocketHub();

    bool Start(unsigned short port);
    void Stop();

    bool IsRunning() const;

    void PollEvents(std::vector<Event>& outEvents);
    bool SendPacket(const NetAddress& target, const std::vector<std::uint8_t>& payload);

private:
    bool InitializeWinsock();
    void ShutdownWinsock();
    bool SetSocketNonBlocking(SOCKET socketHandle);

private:
    bool mWinsockInitialized = false;
    bool mRunning = false;
    SOCKET mSocket = INVALID_SOCKET;
};