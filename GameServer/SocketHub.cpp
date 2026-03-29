#include "SocketHub.h"

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr int kBacklog = SOMAXCONN;
    constexpr std::uint32_t kHeaderSize = 4;
    constexpr int kTempReadSize = 4096;

    void WriteLengthPrefix(std::vector<std::uint8_t>& buffer, std::uint32_t length)
    {
        buffer.push_back(static_cast<std::uint8_t>((length >> 24) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((length >> 16) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>((length >> 8) & 0xFF));
        buffer.push_back(static_cast<std::uint8_t>(length & 0xFF));
    }

    std::uint32_t ReadLengthPrefix(const std::vector<std::uint8_t>& buffer, std::size_t offset)
    {
        return (static_cast<std::uint32_t>(buffer[offset]) << 24) |
            (static_cast<std::uint32_t>(buffer[offset + 1]) << 16) |
            (static_cast<std::uint32_t>(buffer[offset + 2]) << 8) |
            (static_cast<std::uint32_t>(buffer[offset + 3]));
    }
}

SocketHub::SocketHub()
{
}

SocketHub::~SocketHub()
{
    Stop();
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

    mListenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mListenSocket == INVALID_SOCKET)
    {
        ShutdownWinsock();
        return false;
    }

    if (!SetSocketNonBlocking(mListenSocket))
    {
        Stop();
        return false;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(port);

    if (::bind(mListenSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == SOCKET_ERROR)
    {
        Stop();
        return false;
    }

    if (::listen(mListenSocket, kBacklog) == SOCKET_ERROR)
    {
        Stop();
        return false;
    }

    mRunning = true;
    return true;
}

void SocketHub::Stop()
{
    std::unordered_map<int, ClientState>::iterator it = mClients.begin();
    while (it != mClients.end())
    {
        if (it->second.socketHandle != INVALID_SOCKET)
        {
            ::closesocket(it->second.socketHandle);
            it->second.socketHandle = INVALID_SOCKET;
        }
        ++it;
    }

    mClients.clear();

    if (mListenSocket != INVALID_SOCKET)
    {
        ::closesocket(mListenSocket);
        mListenSocket = INVALID_SOCKET;
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

    AcceptPendingConnections(outEvents);
    ReceiveFromClients(outEvents);
}

bool SocketHub::SendPacket(int connectionId, const std::vector<std::uint8_t>& payload)
{
    std::unordered_map<int, ClientState>::iterator it = mClients.find(connectionId);
    if (it == mClients.end())
    {
        return false;
    }

    std::vector<std::uint8_t> framedPacket;
    framedPacket.reserve(kHeaderSize + payload.size());

    WriteLengthPrefix(framedPacket, static_cast<std::uint32_t>(payload.size()));
    framedPacket.insert(framedPacket.end(), payload.begin(), payload.end());

    return SendAll(it->second.socketHandle, &framedPacket[0], static_cast<int>(framedPacket.size()));
}

void SocketHub::DisconnectClient(int connectionId)
{
    RemoveClient(connectionId);
}

std::vector<int> SocketHub::GetConnectionIds() const
{
    std::vector<int> ids;
    ids.reserve(mClients.size());

    std::unordered_map<int, ClientState>::const_iterator it = mClients.begin();
    while (it != mClients.end())
    {
        ids.push_back(it->first);
        ++it;
    }

    return ids;
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

void SocketHub::AcceptPendingConnections(std::vector<Event>& outEvents)
{
    while (true)
    {
        sockaddr_in clientAddress{};
        int clientAddressLength = sizeof(clientAddress);

        SOCKET clientSocket = ::accept(
            mListenSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientAddressLength
        );

        if (clientSocket == INVALID_SOCKET)
        {
            int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                break;
            }

            break;
        }

        if (!SetSocketNonBlocking(clientSocket))
        {
            ::closesocket(clientSocket);
            continue;
        }

        int connectionId = mNextConnectionId++;

        ClientState client{};
        client.socketHandle = clientSocket;
        mClients.insert(std::make_pair(connectionId, client));

        Event event{};
        event.type = EventType::Connected;
        event.connectionId = connectionId;
        outEvents.push_back(event);
    }
}

void SocketHub::ReceiveFromClients(std::vector<Event>& outEvents)
{
    std::vector<int> disconnectedIds;
    std::uint8_t tempBuffer[kTempReadSize]{};

    std::unordered_map<int, ClientState>::iterator it = mClients.begin();
    while (it != mClients.end())
    {
        int connectionId = it->first;
        ClientState& client = it->second;

        while (true)
        {
            int bytesRead = ::recv(
                client.socketHandle,
                reinterpret_cast<char*>(tempBuffer),
                kTempReadSize,
                0
            );

            if (bytesRead > 0)
            {
                client.receiveBuffer.insert(
                    client.receiveBuffer.end(),
                    tempBuffer,
                    tempBuffer + bytesRead
                );

                while (client.receiveBuffer.size() >= kHeaderSize)
                {
                    std::uint32_t payloadLength = ReadLengthPrefix(client.receiveBuffer, 0);

                    if (client.receiveBuffer.size() < kHeaderSize + payloadLength)
                    {
                        break;
                    }

                    Event packetEvent{};
                    packetEvent.type = EventType::PacketReceived;
                    packetEvent.connectionId = connectionId;
                    packetEvent.payload.assign(
                        client.receiveBuffer.begin() + kHeaderSize,
                        client.receiveBuffer.begin() + kHeaderSize + payloadLength
                    );
                    outEvents.push_back(packetEvent);

                    client.receiveBuffer.erase(
                        client.receiveBuffer.begin(),
                        client.receiveBuffer.begin() + kHeaderSize + payloadLength
                    );
                }
            }
            else if (bytesRead == 0)
            {
                disconnectedIds.push_back(connectionId);
                break;
            }
            else
            {
                int error = ::WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                {
                    break;
                }

                disconnectedIds.push_back(connectionId);
                break;
            }
        }

        ++it;
    }

    for (std::size_t i = 0; i < disconnectedIds.size(); ++i)
    {
        Event disconnectEvent{};
        disconnectEvent.type = EventType::Disconnected;
        disconnectEvent.connectionId = disconnectedIds[i];
        outEvents.push_back(disconnectEvent);

        RemoveClient(disconnectedIds[i]);
    }
}

bool SocketHub::SendAll(SOCKET socketHandle, const std::uint8_t* data, int byteCount)
{
    int totalSent = 0;

    while (totalSent < byteCount)
    {
        int sent = ::send(
            socketHandle,
            reinterpret_cast<const char*>(data + totalSent),
            byteCount - totalSent,
            0
        );

        if (sent == SOCKET_ERROR)
        {
            int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                continue;
            }

            return false;
        }

        if (sent <= 0)
        {
            return false;
        }

        totalSent += sent;
    }

    return true;
}

void SocketHub::RemoveClient(int connectionId)
{
    std::unordered_map<int, ClientState>::iterator it = mClients.find(connectionId);
    if (it == mClients.end())
    {
        return;
    }

    if (it->second.socketHandle != INVALID_SOCKET)
    {
        ::closesocket(it->second.socketHandle);
        it->second.socketHandle = INVALID_SOCKET;
    }

    mClients.erase(it);
}