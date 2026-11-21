#include "SocketHub.h"

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr int kTempReadSize = 4096;
}

SocketHub::SocketHub()
{
}

SocketHub::~SocketHub()
{
    Stop();
}

bool SocketHub::InitializeWinsock()
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

void SocketHub::ShutdownWinsock()
{
    if (!mWinsockInitialized)
    {
        return;
    }

    ::WSACleanup();
    mWinsockInitialized = false;
}

bool SocketHub::SetSocketNonBlocking(SOCKET socketHandle)
{
    u_long nonBlocking = 1;
    return ::ioctlsocket(socketHandle, FIONBIO, &nonBlocking) == 0;
}

bool SocketHub::Start(unsigned short port)
{
    if (mRunning)
    {
        return true;
    }

    if (!InitializeWinsock())
    {
        return false;
    }

    mSocket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (mSocket == INVALID_SOCKET)
    {
        ShutdownWinsock();
        return false;
    }

    if (!SetSocketNonBlocking(mSocket))
    {
        Stop();
        return false;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(port);

    if (::bind(mSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        Stop();
        return false;
    }

    mRunning = true;
    return true;
}

void SocketHub::Stop()
{
    if (mSocket != INVALID_SOCKET)
    {
        ::closesocket(mSocket);
        mSocket = INVALID_SOCKET;
    }

    mRunning = false;
    ShutdownWinsock();
}

bool SocketHub::IsRunning() const
{
    return mRunning;
}

void SocketHub::PollEvents(std::vector<Event>& outEvents)
{
    outEvents.clear();

    if (!mRunning)
    {
        return;
    }

    std::uint8_t tempBuffer[kTempReadSize]{};

    while (true)
    {
        sockaddr_in from{};
        int fromLen = sizeof(from);

        int bytesRead = ::recvfrom(
            mSocket,
            reinterpret_cast<char*>(tempBuffer),
            kTempReadSize,
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (bytesRead > 0)
        {
            Event e{};
            e.from.addr = from;
            e.payload.assign(tempBuffer, tempBuffer + bytesRead);
            outEvents.push_back(std::move(e));
        }
        else
        {
            int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                break;
            }

            break;
        }
    }
}

bool SocketHub::SendPacket(const NetAddress& target, const std::vector<std::uint8_t>& payload)
{
    if (!mRunning || mSocket == INVALID_SOCKET)
    {
        return false;
    }

    int sent = ::sendto(
        mSocket,
        reinterpret_cast<const char*>(payload.data()),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<const sockaddr*>(&target.addr),
        sizeof(target.addr)
    );

    return sent == static_cast<int>(payload.size());
}