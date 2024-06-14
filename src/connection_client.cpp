#include "connection_client.h"
#include <iostream>
#include <thread>
#include <numeric>
#include <syncstream>

namespace connection {

ConnectionClient::ConnectionClient(unsigned int clientId, sockpp::tcp_socket sock) : clientId(clientId) {
	ip = sock.peer_address().to_string();
	// Create a thread and transfer the new stream to it.
	clientThread = std::jthread(clientThreadFunc, std::ref(*this), std::move(sock));
	active = true;
	// Mark the time at which the client was connected
	connectionTime = std::chrono::system_clock::now();

}

bool ConnectionClient::runTransaction(ClientTransaction& transaction) {
	if (active) {
		transaction.state = SCHEDULED;
		// lock access to the clients transactions (RAII)
		const std::lock_guard<std::mutex> lock(transactionsMutex);
		transactions.push_back(transaction);
		return true;
	}
	return false;
}

void ConnectionClient::notifyRtt(std::chrono::milliseconds rtt) {
	rttLog.push_back(rtt.count());
	while (rttLog.size() > 10) {
		rttLog.pop_front();
	}
}

std::chrono::milliseconds ConnectionClient::getRtt() const {

	if (rttLog.size() > 0) {
		unsigned int sum = std::accumulate(rttLog.begin(), rttLog.end(), 0);
		return std::chrono::milliseconds(sum / rttLog.size());
	}
	return std::chrono::milliseconds(0);
}


std::chrono::milliseconds ConnectionClient::getConnectionTime() const {
	auto currentTime = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - connectionTime);
}


std::chrono::milliseconds ConnectionClient::getDisconnectionTime() const {
	if (active) {
		return std::chrono::milliseconds(0);
	} else {
		auto currentTime = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - disconnectionTime);
	}
}



void ConnectionClient::clientThreadFunc(std::stop_token stop_token, ConnectionClient& client, sockpp::tcp_socket sock) {

	// we will use blocking mode for socket read, but we must rely on timeouts
    if (false == sock.read_timeout(std::chrono::milliseconds(500))) {
    	std::osyncstream(std::cerr) << "Unable to work with timeout-less sockets. Aborting" << std::endl;
    	abort();
    }

    std::osyncstream(std::cout) << "Got remote connection from " << sock.peer_address() << std::endl;

	// we are using stop_token of std::jthread to check if stop was requested
	while (!stop_token.stop_requested()) {
		// check, if there are some transactions waiting
		// lock access to the transactions
		client.transactionsMutex.lock();
		if (false == client.transactions.empty()) {
			ClientTransaction& transaction = client.transactions.front();
			// remove this transaction from the queue
			client.transactions.pop_front();
			// unlock access to the transactions
			client.transactionsMutex.unlock();

			if (SCHEDULED == transaction.state) {
				//std::osyncstream(std::cout) << "Running transaction with " << sock.peer_address() << ", sending " << transaction.request.size() << " bytes, expecting " << transaction.responseSize << std::endl;
				// Mark request time for RTT calculation
				transaction.requestTime = std::chrono::system_clock::now();
				// Try to write data to socket
				if (transaction.request.size() != sock.write_n(transaction.request.data(), transaction.request.size())) {
					// we were unable to write data to socket - treat this as timeout
					transaction.state = TIMEOUT;
					// signal that the transaction is finished
					transaction.endOfTransactionSignal.release();
					// no data could be sent - this means that the socket was closed - we need to close the connection thread
					client.clientThread.request_stop();
				} else {
					// Check if we need to wait for response
					if (transaction.responseSize > 0) {
						// we should wait for the response
						transaction.state = WAITING;
						if (transaction.responseSize == sock.read(transaction.responseBuf, transaction.responseSize)) {
							// validate the response
							if (true == transaction.validator(client.getClientId(), std::span<const uint8_t>(transaction.responseBuf, transaction.responseSize))) {
								// response is valid
								transaction.state = DONE;
							} else {
								// response is invalid
								transaction.responseSize = 0;
								transaction.state = TIMEOUT;
								std::osyncstream(std::cout) << "Got invalid response from " << sock.peer_address() << std::endl;
							}
						} else {
							transaction.state = TIMEOUT;
						}
						// Mark response time for RTT calculation
						transaction.responseTime = std::chrono::system_clock::now();
						// Calculate RTT and notify the client object about it
						transaction.rtt = std::chrono::duration_cast<std::chrono::milliseconds>(transaction.responseTime - transaction.requestTime);
						client.notifyRtt(transaction.rtt);
						// signal that the transaction is finished
						transaction.endOfTransactionSignal.release();
						if (transaction.responseSize == 0) {
							// no data could be read - this means that the socket was closed - we need to close the connection thread
							client.clientThread.request_stop();
						}
					} else {
						// there is no expected response - the transaction is done
						transaction.state = DONE;
						// signal that the transaction is finished
						transaction.endOfTransactionSignal.release();
					}
				}
			}
		} else {
			// unlock access to the transactions
			client.transactionsMutex.unlock();
			Sleep(10);
		}
	}
	std::osyncstream(std::cout) << "Closing remote connection with " << sock.peer_address() << std::endl;
	// Mark the time at which the client was connected
	client.disconnectionTime = std::chrono::system_clock::now();
	client.active = false;
}


} // namespace connection
