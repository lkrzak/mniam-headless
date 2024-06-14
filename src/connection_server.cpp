#include "connection_server.h"

#include <iostream>
#include <span>
#include <syncstream>

namespace connection {

    Server::Server(uint16_t listenPortNo, size_t clientLimit) : isAccepting(true), clientLimit(clientLimit) {
        // initialize sockpp library
        sockpp::initialize();
        // run the server thread
        serverThread = std::thread(serverThreadFunc, this, listenPortNo);
        serverThread.detach();
    }

    void Server::runTransaction(Transaction& transaction) {
        // lock access to the clients list (RAII)
        const std::lock_guard<std::mutex> lock(clientsMutex);

        // reset the transaction
        transaction.reset();

        for (auto& client : clients) {
            // schedule transactions with active clients only
            if (client.second.isActive()) {
                // construct individual client transaction
                auto clientTransaction = transaction.clientTransactions.try_emplace(client.first, transaction.request, transaction.responseSize, transaction.validator);
                // and run it (clientTransaction is an std::pair<iterator, bool>)
                client.second.runTransaction(clientTransaction.first->second);
            }
        }
    }


    void Server::runTransactionWithSingleClient(unsigned int clientId, Transaction& transaction) {
        // lock access to the clients list (RAII)
        const std::lock_guard<std::mutex> lock(clientsMutex);

        // reset the transaction
        transaction.reset();

        // find client
        auto& client = clients.at(clientId);
        if (client.isActive()) {
            // construct individual client transaction
            auto clientTransaction = transaction.clientTransactions.try_emplace(client.getClientId(), transaction.request, transaction.responseSize, transaction.validator);
            // and run it (clientTransaction is an std::pair<iterator, bool>)
            client.runTransaction(clientTransaction.first->second);
        }
    }

    void Server::removeClient(unsigned int clientId) {
        // lock access to the clients list (RAII)
        const std::lock_guard<std::mutex> lock(clientsMutex);

        // Erase the client object from the map. This sould destroy the client object and in turn safely close the client connection
        clients.erase(clientId);
    }

    void Server::removeAllInactiveClients() {
        // lock access to the clients list (RAII)
        const std::lock_guard<std::mutex> lock(clientsMutex);

        // erase all clients from the map for which the isActive returns false
        const auto count = std::erase_if(clients, [](const auto& item) {
            auto const& [key, value] = item;
            return value.isActive() == false;
        });
    }


    const std::vector<ClientInfo> Server::getClients() {
        // lock access to the clients list (RAII)
        const std::lock_guard<std::mutex> lock(clientsMutex);

        std::vector<ClientInfo> clientInfo;
        for (auto& client : clients) {
            clientInfo.emplace_back(client.first, client.second.isActive(), client.second.getIP(), client.second.getRtt(), client.second.getConnectionTime(), client.second.getDisconnectionTime());
        }

        return std::move(clientInfo);
    }

    ClientInfo Server::getClient(unsigned int clientId) {
        // lock access to the clients list (RAII)
        const std::lock_guard<std::mutex> lock(clientsMutex);
        try {
            auto& client = clients.at(clientId);
            return ClientInfo(client.getClientId(), client.isActive(), client.getIP(), client.getRtt(), client.getConnectionTime(), client.getDisconnectionTime());
        } catch (...) {
        }
        return ClientInfo(clientId, false, "unknown", std::chrono::milliseconds(0), std::chrono::milliseconds(0), std::chrono::milliseconds(0));
    }

    void Server::rejectIncomingConnections() {
        isAccepting = false;
    }

    void Server::acceptIncomingConnections() {
        isAccepting = true;
    }

    void Server::serverThreadFunc(Server* server, uint16_t listenPortNo) {
        static unsigned int clientId;

        std::osyncstream(std::cout) << "Server thread started" << std::endl;
        sockpp::tcp_acceptor acc(listenPortNo);

        while (true) {
            std::osyncstream(std::cout) << "Waiting for connections" << std::endl;
            // Accept a new client connection
            sockpp::tcp_socket sock = acc.accept();

            if (!sock) {
                std::osyncstream(std::cerr) << "Error accepting incoming connection: " << acc.last_error_str() << std::endl;
                break;
            } else {
                if ((server->isAccepting) && (server->clients.size() < server->clientLimit)) {
                    std::osyncstream(std::cout) << "Server thread: incoming connection accepted" << std::endl;

                    // lock access to the clients list (RAII)
                    const std::lock_guard<std::mutex> lock(server->clientsMutex);

                    // Create new client instance and move the socket there
                    server->clients.emplace(std::piecewise_construct, std::forward_as_tuple(clientId), std::forward_as_tuple(clientId, std::move(sock)));
                    clientId++;
                } else {
                    std::osyncstream(std::cout) << "Server thread: incoming connection rejected" << std::endl;
                }
            }
        }
    }

} // namespace connection
