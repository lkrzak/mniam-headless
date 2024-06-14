#ifndef AMGAME_REMOTE_CONNECTION_H_
#define AMGAME_REMOTE_CONNECTION_H_
#pragma once

#include "amcom.h"
#include "amcom_packets.h"

#include <winsock2.h>

#include <chrono>
#include <iostream>
#include <queue>
#include <string>

namespace amgame {
    class RemoteConnectionTransaction {
      private:
        AMCOM_PacketType request{AMCOM_NO_PACKET};
        AMCOM_PacketType confirmation{AMCOM_NO_PACKET};

        using time_point = decltype(std::chrono::system_clock::now());

        time_point start;
        time_point end;

        enum class State {
            IDLE,
            REQUEST_TO_SEND,
            AWAITING_CONFIRMATION,
            FINISHED,

        } state{State::IDLE};

      public:
        void setupTransaction(AMCOM_PacketType request_, AMCOM_PacketType confirmation_) {
            request      = request_;
            confirmation = confirmation_;
            state        = State::REQUEST_TO_SEND;
        }

        [[nodiscard]] AMCOM_PacketType getRequestToSend() const {
            return (State::REQUEST_TO_SEND == state) ? request : AMCOM_NO_PACKET;
        }

        void indicateTransmission() {
            if (confirmation != AMCOM_NO_PACKET) {
                state = State::AWAITING_CONFIRMATION;
                start = std::chrono::system_clock::now();
            } else {
                state = State::FINISHED;
            }
        }

        void indicateReception(AMCOM_PacketType receivedPacketType) {
            if ((State::AWAITING_CONFIRMATION == state) && (receivedPacketType == confirmation)) {
                state         = State::FINISHED;
                end           = std::chrono::system_clock::now();
                auto duration = end - start;
                std::cout << "Transaction handled in " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << "ms\n";
            }
        }

        [[nodiscard]] bool isFinished() const {
            return (State::FINISHED == state);
        }
    };

    class RemoteConnection {
      private:
        SOCKET                      connectionSocket;       ///< TCP connection socket used to communicate with remote player
        AMCOM_Receiver              amReceiver;             ///< AMCOM packet receiver instance
        HANDLE                      connectionThreadHandle; ///< Connection thread handle
        volatile bool               alive;                  ///< Flag indicating whether the connection with remote player is alive
        bool                        identified;             ///< Flag indicating whether the player was identified
        RemoteConnectionTransaction remoteTransaction;

        char    playerName[64]{}; ///< Player name as returned by IDENTIFY.confirmation
        float   moveAngle;        ///< Player move angle as returned by MOVE.confirmation
        uint8_t playerNumber;
        uint8_t numberOfPlayers;
        float   mapWidth;
        float   mapHeight;
        float   x;
        float   y;

        static DWORD WINAPI connectionThread(LPVOID lpParam);
        static void         amPacketHandler(const AMCOM_Packet* packet, void* userContext);
        void                packetReceived(const AMCOM_Packet* packet);

      public:
        std::string ip;   ///< IP address of the remote player
        std::string port; ///< port used on the remote player side

        RemoteConnection(const char* ip_, const char* port_) : ip(ip_), port(port_), connectionSocket(INVALID_SOCKET), alive(false), identified(false), connectionThreadHandle(0) {
        }

        ~RemoteConnection() = default;

        // Move and copy MUST be disabled for proper socket operation
        RemoteConnection(RemoteConnection const&)            = delete;
        RemoteConnection(RemoteConnection&&)                 = delete;
        RemoteConnection& operator=(RemoteConnection const&) = delete;
        RemoteConnection& operator=(RemoteConnection&&)      = delete;

        // static void initializeSockets();
        // static void deinitializeSockets();

        std::queue<AMCOM_PlayerState> playerStateUpdateQueue;
        std::queue<AMCOM_FoodState>   foodStateUpdateQueue;

        void connectPlayer();
        void disconnectPlayer();
        void waitForDisconnection();
        bool identify();
        bool newGame(uint8_t playerNo, uint8_t numberOfPlayers, float mapWidth, float mapHeight);
        bool sendMoveRequest(float x, float y);
        bool sendPlayerUpdate();
        bool sendFoodUpdate();


        [[nodiscard]] bool isAlive() const {
            return alive;
        }


        // [[nodiscard]] bool isAlive() const {
        //     return alive;
        // }

        [[nodiscard]] bool isIdentified() const {
            return identified;
        }

        [[nodiscard]] bool isTransactionFinished() const {
            return remoteTransaction.isFinished();
        }

        [[nodiscard]] char const* getPlayerName() const {
            if (identified) {
                return const_cast<const char*>(playerName);
            }
            return nullptr;
        }

        [[nodiscard]] float getPlayerMoveAngle() const {
            return moveAngle;
        }
    };

} // namespace amgame


#endif /* AMGAME_REMOTE_CONNECTION_H_ */
