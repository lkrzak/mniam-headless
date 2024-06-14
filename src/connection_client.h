#ifndef CONNECTION_CLIENT_H_
#define CONNECTION_CLIENT_H_

#include "sockpp/tcp_socket.h"
#include <thread>
#include <deque>
#include <queue>
#include <span>
#include <semaphore>
#include <chrono>

namespace connection {

enum ConnectionTransactionState{
	IDLE = 0,
	SCHEDULED,
	WAITING,
	DONE,
	TIMEOUT
};

class TransactionResponseValidator {
public:
	virtual bool operator()(unsigned int clientId, std::span<const uint8_t> responseData) = 0;
	virtual ~TransactionResponseValidator() { ; }
};

class DefaultTransactionResponseValidator : public TransactionResponseValidator {
public:
	virtual bool operator()(unsigned int clientId, std::span<const uint8_t> responseData) { return true; }
	virtual ~DefaultTransactionResponseValidator() { ; }
};

static DefaultTransactionResponseValidator defaultTransactionResponseValidator;

/** Represents a single transaction (request-response) with a single remote client */
class ClientTransaction {
	friend class ConnectionClient;
public:
	/**
	 * Constructs a transaction.
	 *
	 * @param[in] requestData data to be sent as a request
	 * @param[in] expectedResponseSize expected size (in bytes) of the response. If set to 0, the transaction will not wait for the response
	 */
	ClientTransaction (std::span<const uint8_t> requestData, std::size_t expectedResponseSize = 0, TransactionResponseValidator& responseValidator = defaultTransactionResponseValidator) : request(requestData), responseSize(expectedResponseSize), response({responseBuf, responseSize}), validator(responseValidator) {
		;
	}

	/**
	 * Waits for the end of the transaction until a specified moment in time.
	 * @param absTime absTime absolute moment in time when timeout will occur if no response to the request is received
	 * @retval true if the transaction has successfully finished
	 * @retval false if the transaction did not finish successfully
	 */
	template<class Clock, class Duration>
	bool waitUntil( const std::chrono::time_point<Clock, Duration>& absTime ) {
		if (endOfTransactionSignal.try_acquire_until(absTime)) {
			if (state == DONE) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Gets the response to the request.
	 * @return response data
	 */
	std::span<const uint8_t> getResponse() {
		if (state == DONE) {
			// return a subspan of the response
			return response.subspan(0, responseSize);
		}
		// return empty span
		return std::span<const uint8_t>();
	}


private:
	/// place to store the response
	uint8_t responseBuf[512];
	/// transaction state
	volatile ConnectionTransactionState state{IDLE};
	/// request data
	std::span<const uint8_t> request;
	/// response size
	std::size_t responseSize;
	/// response data
	std::span<const uint8_t> response;
	/// response validator
	TransactionResponseValidator& validator;
	/// semaphore used to signal the end of transaction
	std::binary_semaphore endOfTransactionSignal{0};
	/// time at which request was sent
	std::chrono::time_point<std::chrono::system_clock> requestTime;
	/// time at which response was received
	std::chrono::time_point<std::chrono::system_clock> responseTime;
	/// calculated round trip time
	std::chrono::milliseconds rtt;
};

class ConnectionClient {
public:
	ConnectionClient(unsigned int clientId, sockpp::tcp_socket sock);
	bool isActive(void) const { return active; }
	bool runTransaction(ClientTransaction& transaction);
	unsigned int getClientId() const { return clientId; }
	std::string getIP() const { return ip; }
	std::chrono::milliseconds  getRtt() const;
	std::chrono::milliseconds getConnectionTime() const ;
	std::chrono::milliseconds getDisconnectionTime() const ;
	void notifyRtt(std::chrono::milliseconds rtt);
private:
	unsigned int clientId;
	/// Client connection state
	bool active;
	/// Client IP
	std::string ip;
	/// Connection thread
	std::jthread clientThread;
	/// Queue of transactions
	std::deque<std::reference_wrapper<ClientTransaction>> transactions;
	/// Mutex guarding access to transactions
	std::mutex transactionsMutex;
	/// Mean RTT (round trip time)
	std::deque<unsigned int> rttLog;
	/// Time at which the client was connected
	std::chrono::time_point<std::chrono::system_clock> connectionTime;
	/// Time at which the client was disconnected
	std::chrono::time_point<std::chrono::system_clock> disconnectionTime;
	static void clientThreadFunc(std::stop_token stop_token, ConnectionClient& client, sockpp::tcp_socket sock);
};

}

#endif /* CONNECTION_CLIENT_H_ */
