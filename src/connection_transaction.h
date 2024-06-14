#ifndef CONNECTION_TRANSACTION_H_
#define CONNECTION_TRANSACTION_H_

#include <cstdint>
#include "connection_client.h"
#include "sockpp/tcp_acceptor.h"
#include <span>
#include <map>
#include <thread>
#include <mutex>

namespace connection {

/**
 * This class represents a transaction (request-response) between a server and one or more remote clients.
 * The transaction is always initiated by the server sending request data.
 * The transaction may also handle the response.
 */
class Transaction {
friend class Server;
public:
	/**
	 * Creates a transaction.
	 *
	 * @param[in] requestData data that will be sent as a request
	 * @param[in] expectedResponseSize expected size (in bytes) of the response. Value of 0 means that there will be no response
	 * @param[in] responseValidator response validation object used to validate the response
	 */
	Transaction (std::span<const uint8_t> requestData, std::size_t expectedResponseSize = 0, TransactionResponseValidator& responseValidator = defaultTransactionResponseValidator) : request(requestData), responseSize(expectedResponseSize), validator(responseValidator) {
		;
	}
	virtual ~Transaction() { ; }

	/**
	 * Waits at most the given time for the transaction to finish.
	 */
	template< class Rep, class Period >
	std::size_t waitForFinish(const std::chrono::duration<Rep, Period>& rel_time ) {
		const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

		std::size_t successCount = 0;

		for (auto& clientTransaction : clientTransactions) {
			if (true == clientTransaction.second.waitUntil(now + rel_time)) {
				successCount++;
			}
		}
		return successCount;
	}

	/**
	 * Gets the response from a client with the given clientId. Usually called after waitForFinish.
	 */
	std::span<const uint8_t> getResponse(unsigned int clientId) {

		// return empty span
		return std::span<const uint8_t>();
	}

	/**
	 * Resets the transaction, so it can be run again.
	 */
	virtual void reset() {
		clientTransactions.clear();
	}

protected:
	/// Request data to be sent to all clients during the transaction
	std::span<const uint8_t> request;
	/// Expected response size
	std::size_t responseSize;
	/// Vector of client transactions
	std::map<unsigned int, ClientTransaction> clientTransactions;
	/// Transaction response validator used to validate the response
	TransactionResponseValidator& validator;
};

} // namespace


#endif /* CONNECTION_TRANSACTION_H_ */
