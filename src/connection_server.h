#ifndef CONNECTION_SERVER_H_
#define CONNECTION_SERVER_H_

#include "connection_client.h"
#include "connection_transaction.h"
#include "sockpp/tcp_acceptor.h"

#include <atomic>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>
#include <thread>

namespace connection {

    /**
     * Structure representing information snapshot about a single remote client.
     */
    struct ClientInfo {
        /// Client identifier
        unsigned int clientId;
        /// True if connection with this client is active
        bool active;
        /// IP address and port of the client
        std::string ip;
        /// Mean round trip time in ms
        std::chrono::milliseconds rtt;
        /// Number of milliseconds the client was connected
        std::chrono::milliseconds howLongConnected;
        // Number of milliseconds the client was disconnected
        std::chrono::milliseconds howLongDisconnected;
    };

    /**
     * Represents a connection server listening on a given TCP/IP port.
     *
     * The job of the server is to listen on a given port and accept connections from remote clients.
     * Upon connection, each client is assigned a unique clientId.
     */
    class Server {
      public:
        /**
         * Constructs and runs a server listening on the given port number.
         * @param[in] listenPortNo port number where the server will listen for incoming connections from clients
         */
        Server(uint16_t listenPortNo, size_t clientLimit = 100);

        /**
         * Runs a transaction with all the clients.
         *
         * @param[in, out] transaction transaction to be run
         */
        void runTransaction(Transaction& transaction);

        /**
         * Runs a transaction with a single specified client.
         *
         * @param[in] clientId client ID
         * @param[in, out] transaction transaction to be run
         */
        void runTransactionWithSingleClient(unsigned int clientId, Transaction& transaction);

        /**
         * Removes a client from the server. If the client is connected, it will be first disconnected.
         *
         * @param[in] clientId client ID
         */
        void removeClient(unsigned int clientId);

        /**
         * Removes all inactive clients.
         */
        void removeAllInactiveClients();

        /**
         * Returns a vector of client descriptions. This is a snapshot of the current state.
         */
        const std::vector<ClientInfo> getClients();

        /**
         * Returns a single client description.
         */
        ClientInfo getClient(unsigned int clientId);

        /**
         * Makes the server reject all further incoming connections.
         */
        void rejectIncomingConnections();

        /**
         * Makes the server accept again all further incoming connections.
         */
        void acceptIncomingConnections();

      private:
        /// Server thread instance
        std::thread serverThread;
        /// Map of clients. Each client is identified by a unique id (unsigned int)
        std::map<unsigned int, ConnectionClient> clients;
        /// Protects access to the clients
        std::mutex clientsMutex;
        /// Flag that controls if the server is accepting incoming connections (true) or not (false)
        std::atomic<bool> isAccepting;
        /// Limit on the number of accepted clients
        size_t clientLimit;
        /**
         * Implementation of the server thread.
         * @param[in] server server instance
         * @param[in] listenPortNo listening port number
         */
        static void serverThreadFunc(Server* server, uint16_t listenPortNo);
    };

} // namespace connection


#endif /* CONNECTION_SERVER_H_ */
