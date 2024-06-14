#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include "remote_connection.h"

#include "amcom_packets.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>


namespace amgame {

    /// Performs WinSock initialization/deinitialization in RAII manner
    struct SocketResource {
      public:
        ~SocketResource() {
            printf("WSACleanup\n");
            WSACleanup();
        }

        static auto& instance() {
            static SocketResource instance;
            return instance.wsaData;
        }

      protected:
        static inline WSADATA wsaData{};

        SocketResource() {
            // Initialize Winsock
            if (auto r = WSAStartup(MAKEWORD(2, 2), &wsaData); r != 0) {
                throw std::runtime_error("WSAStartup failed");
            }
        }
    };


    static auto& socketResource{SocketResource::instance()}; ///< Forces WinSock to be initialized at program start


    void RemoteConnection::connectPlayer() {
        alive                  = false;
        identified             = false;
        connectionThreadHandle = CreateThread(NULL, 0, RemoteConnection::connectionThread, this, 0, NULL);
    }


    void RemoteConnection::disconnectPlayer() {
        alive      = false;
        identified = false;
    }

    void RemoteConnection::waitForDisconnection() {
        if (connectionThreadHandle != nullptr) {
            WaitForSingleObject(connectionThreadHandle, 3000);
            connectionThreadHandle = nullptr;
        }
    }

    void RemoteConnection::amPacketHandler(const AMCOM_Packet* packet, void* userContext) {
        reinterpret_cast<RemoteConnection*>(userContext)->packetReceived(packet);
    }

    DWORD WINAPI RemoteConnection::connectionThread(LPVOID lpParam) {
        RemoteConnection* player = reinterpret_cast<RemoteConnection*>(lpParam);
        AMCOM_InitReceiver(&player->amReceiver, RemoteConnection::amPacketHandler, lpParam);
        uint8_t          packet[AMCOM_MAX_PACKET_SIZE];
        size_t           packetSize;
        int              socketResult;
        SOCKET           conectionSocket;
        struct addrinfo *result = NULL, *ptr = NULL, hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;


        // memset(player->playerName, 0x00, sizeof(player->playerName));

        // Resolve the server address and port
        int iResult = getaddrinfo(player->ip.c_str(), player->port.c_str(), &hints, &result);
        if (iResult != 0) {
            printf("getaddrinfo failed with error: %d\n", iResult);
            return false;
        }

        // Attempt to connect to an address until one succeeds
        for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {
            // Create a SOCKET for connecting to server
            player->connectionSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (player->connectionSocket == INVALID_SOCKET) {
                printf("socket failed with error: %ld\n", WSAGetLastError());
                return 1;
            }

            // Connect to server.
            iResult = connect(player->connectionSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
            if (iResult == SOCKET_ERROR) {
                closesocket(player->connectionSocket);
                player->connectionSocket = INVALID_SOCKET;
                continue;
            }
            break;
        }

        freeaddrinfo(result);

        if (player->connectionSocket == INVALID_SOCKET) {
            printf("Unable to connect to server!\n");
            return 1;
        }

        player->alive = true;

        while (player->alive) {
            // check if we need to start new transaction
            AMCOM_PacketType request = player->remoteTransaction.getRequestToSend();
            switch (request) {
                case AMCOM_NO_PACKET:
                    // nothing to do
                    break;
                case AMCOM_IDENTIFY_REQUEST: {
                    // Prepare IDENTIFY.request
                    AMCOM_IdentifyRequestPayload identifyRequest;
                    identifyRequest.gameVerHi    = 0;
                    identifyRequest.gameVerLo    = 1;
                    identifyRequest.gameRevision = 0;
                    packetSize                   = AMCOM_Serialize(AMCOM_IDENTIFY_REQUEST, &identifyRequest, sizeof(identifyRequest), packet);
                    // Send the request
                    socketResult = send(player->connectionSocket, reinterpret_cast<const char*>(packet), packetSize, 0);
                    if (socketResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        // end this communication thread
                        player->alive = false;
                    }
                    player->remoteTransaction.indicateTransmission();
                } break;
                case AMCOM_NEW_GAME_REQUEST: {
                    // Prepare NEW_GAME.request
                    AMCOM_NewGameRequestPayload newGameRequest;
                    newGameRequest.playerNumber    = player->playerNumber;
                    newGameRequest.numberOfPlayers = player->numberOfPlayers;
                    newGameRequest.mapWidth        = player->mapWidth;
                    newGameRequest.mapHeight       = player->mapHeight;
                    packetSize                     = AMCOM_Serialize(AMCOM_NEW_GAME_REQUEST, &newGameRequest, sizeof(newGameRequest), packet);
                    // Send the request
                    socketResult = send(player->connectionSocket, reinterpret_cast<const char*>(packet), packetSize, 0);
                    if (socketResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        // end this communication thread
                        player->alive = false;
                    }
                    player->remoteTransaction.indicateTransmission();
                } break;
                case AMCOM_PLAYER_UPDATE_REQUEST: {
                    // Prepare PLAYER_UPDATE.request
                    AMCOM_PlayerUpdateRequestPayload playerUpdateRequest;
                    size_t                           p = 0;
                    while ((player->playerStateUpdateQueue.size()) && (p < AMCOM_MAX_PLAYER_UPDATES)) {
                        playerUpdateRequest.playerState[p] = player->playerStateUpdateQueue.front();
                        player->playerStateUpdateQueue.pop();
                        p++;
                    }
                    packetSize = AMCOM_Serialize(AMCOM_PLAYER_UPDATE_REQUEST, &playerUpdateRequest, p * sizeof(AMCOM_PlayerState), packet);
                    // Send the request
                    socketResult = send(player->connectionSocket, reinterpret_cast<const char*>(packet), packetSize, 0);
                    if (socketResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        // end this communication thread
                        player->alive = false;
                    }
                    player->remoteTransaction.indicateTransmission();
                } break;
                case AMCOM_FOOD_UPDATE_REQUEST: {
                    // Prepare FOOD_UPDATE.request
                    AMCOM_FoodUpdateRequestPayload foodUpdateRequest;
                    size_t                         f = 0;
                    while ((player->foodStateUpdateQueue.size()) && (f < AMCOM_MAX_FOOD_UPDATES)) {
                        foodUpdateRequest.foodState[f] = player->foodStateUpdateQueue.front();
                        player->foodStateUpdateQueue.pop();
                        f++;
                    }
                    packetSize = AMCOM_Serialize(AMCOM_FOOD_UPDATE_REQUEST, &foodUpdateRequest, f * sizeof(AMCOM_FoodState), packet);
                    // Send the request
                    socketResult = send(player->connectionSocket, reinterpret_cast<const char*>(packet), packetSize, 0);
                    if (socketResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        // end this communication thread
                        player->alive = false;
                    }
                    player->remoteTransaction.indicateTransmission();
                } break;
                case AMCOM_MOVE_REQUEST: {
                    // Prepare MOVE.request
                    AMCOM_MoveRequestPayload moveRequest;
//                    moveRequest.x = player->x;
//                    moveRequest.y = player->y;
                    // printf("Sending MOVE.request\n");
                    packetSize = AMCOM_Serialize(AMCOM_MOVE_REQUEST, &moveRequest, sizeof(moveRequest), packet);
                    // Send the request
                    socketResult = send(player->connectionSocket, reinterpret_cast<const char*>(packet), packetSize, 0);
                    if (socketResult == SOCKET_ERROR) {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        // end this communication thread
                        player->alive = false;
                    }
                    player->remoteTransaction.indicateTransmission();
                } break;
            } // switch

            unsigned long bytesAvailable;

            ioctlsocket(player->connectionSocket, FIONREAD, &bytesAvailable);
            if (bytesAvailable) {
                // check if there is something to receive
                size_t recvSize = recv(player->connectionSocket, reinterpret_cast<char*>(packet), sizeof(packet), 0);
                if (recvSize > 0) {
                    // got some data - deserialize it into AMCOM packet
                    AMCOM_Deserialize(&player->amReceiver, packet, recvSize);
                } else if (recvSize < 0) {
                    printf("Socket error. Closing RemotePlayer::connectionThread\n");
                    player->alive = false;
                }
            } else {
                Sleep(1);
            }
        }

        // shutdown the connection since no more data will be sent
        printf("Shutting down socket\n");
        socketResult = shutdown(player->connectionSocket, SD_BOTH);
        if (socketResult == SOCKET_ERROR) {
            printf("Shutting down socket error\n");
            closesocket(player->connectionSocket);
            return 0;
        }

        // Receive until the peer closes the connection
        do {
            socketResult = recv(player->connectionSocket, reinterpret_cast<char*>(packet), sizeof(packet), 0);
        } while (socketResult > 0);

        closesocket(player->connectionSocket);


        printf("Shutting down socket for real\n");

        player->connectionSocket = INVALID_SOCKET;
        Sleep(2000);
        return 0;
    }


    void RemoteConnection::packetReceived(const AMCOM_Packet* packet) {
        switch (packet->header.type) {
            case AMCOM_IDENTIFY_RESPONSE:
                identified = true;
                memcpy(playerName, reinterpret_cast<const AMCOM_IdentifyResponsePayload*>(packet->payload)->playerName, sizeof(playerName));
                break;
            case AMCOM_MOVE_RESPONSE: moveAngle = reinterpret_cast<const AMCOM_MoveResponsePayload*>(packet->payload)->angle; break;
        }
        // indicate that this packet was received
        remoteTransaction.indicateReception(static_cast<AMCOM_PacketType>(packet->header.type));
    }

    bool RemoteConnection::identify() {
        if (alive) {
            // set up the transaction - the rest is done in the communication thread
            remoteTransaction.setupTransaction(AMCOM_IDENTIFY_REQUEST, AMCOM_IDENTIFY_RESPONSE);
        }
        return alive;
    }

    bool RemoteConnection::newGame(uint8_t playerNo, uint8_t numberOfPlayers, float mapWidth, float mapHeight) {
        if (alive) {
            this->playerNumber    = playerNo;
            this->numberOfPlayers = numberOfPlayers;
            this->mapWidth        = mapWidth;
            this->mapHeight       = mapHeight;
            // set up the transaction - the rest is done in the communication thread
            remoteTransaction.setupTransaction(AMCOM_NEW_GAME_REQUEST, AMCOM_NEW_GAME_RESPONSE);
        }
        return alive;
    }

    bool RemoteConnection::sendMoveRequest(float x, float y) {
        if (alive) {
            this->x = x;
            this->y = y;
            // set up the transaction - the rest is done in the communication thread
            remoteTransaction.setupTransaction(AMCOM_MOVE_REQUEST, AMCOM_MOVE_RESPONSE);
        }
        return alive;
    }

    bool RemoteConnection::sendPlayerUpdate() {
        if (alive) {
            // set up the transaction - the rest is done in the communication thread
            remoteTransaction.setupTransaction(AMCOM_PLAYER_UPDATE_REQUEST, AMCOM_NO_PACKET);
        }
        return alive;
    }

    bool RemoteConnection::sendFoodUpdate() {
        if (alive) {
            // set up the transaction - the rest is done in the communication thread
            remoteTransaction.setupTransaction(AMCOM_FOOD_UPDATE_REQUEST, AMCOM_NO_PACKET);
        }
        return alive;
    }


} // namespace amgame
