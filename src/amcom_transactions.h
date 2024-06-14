
#ifndef AMCOM_TRANSACTIONS_H_
#define AMCOM_TRANSACTIONS_H_

#include "amcom.h"
#include "amcom_packets.h"
#include "connection_server.h"

#include <map>
#include <mutex>
#include <optional>
#include <tuple>

/**
 * This is a transaction template for all AMCOM transactions, that handle both request and response.
 * This class is also a transaction validator, as it implements the required operator()
 */
template<AMCOM_PacketType requestPacket, AMCOM_PacketType responsePacket, typename RequestPayload, typename ResponsePayload> class AMCOMTransaction :
    public connection::Transaction,
    connection::TransactionResponseValidator {
    using AMCOMTransactionContext = std::tuple<bool, AMCOMTransaction<requestPacket, responsePacket, RequestPayload, ResponsePayload>&, unsigned int>;

  public:
    AMCOMTransaction() : connection::Transaction({requestPayload, sizeof(requestPayload)}, AMCOM_PACKET_OVERHEAD + sizeof(ResponsePayload), *this) { ; }
    virtual ~AMCOMTransaction() { ; }

    /**
     * The job of this operator is to validate the incoming responseData.
     */
    virtual bool operator()(unsigned int clientId, std::span<const uint8_t> responseData) {
        AMCOM_Receiver amcomReceiver;
        // prepare context for the deserializer - this is a set of information that will be used by the AMCOM packetHandler
        AMCOMTransactionContext context{false, *this, clientId};
        // prepare AMCOM receiver
        AMCOM_InitReceiver(&amcomReceiver, packetHandler, (void*)&context);
        // deserialize - the rest is done in the packetHandler callback
        AMCOM_Deserialize(&amcomReceiver, responseData.data(), responseData.size());
        // the callback should have set this to true, if packet was ok
        return std::get<0>(context);
    }

    /**
     * Gets the response from a given client.
     */
    std::optional<ResponsePayload> getResponse(unsigned int clientId) {
        std::lock_guard<std::mutex> lock(responsesMutex);
        if (auto element = responses.find(clientId); element != responses.end()) {
            return element->second;
        }
        return {};
    }

    uint8_t requestPayload[AMCOM_PACKET_OVERHEAD + sizeof(RequestPayload)];

  private:
    std::mutex                              responsesMutex;
    std::map<unsigned int, ResponsePayload> responses;

    void saveResponse(unsigned int clientId, ResponsePayload* payload) {
        std::lock_guard<std::mutex> lock(responsesMutex);
        responses.emplace(clientId, *payload);
    }

    static void packetHandler(const AMCOM_Packet* packet, void* userContext) {
        // retrieve the context
        AMCOMTransactionContext& context = *reinterpret_cast<AMCOMTransactionContext*>(userContext);

        if (packet->header.type == responsePacket) {
            // mark that the packet payload is OK
            std::get<0>(context) = true;
            // save the response
            std::get<1>(context).saveResponse(std::get<2>(context), (ResponsePayload*)packet->payload);
        }
    }
};


class IdentifyTransaction : public AMCOMTransaction<AMCOM_IDENTIFY_REQUEST, AMCOM_IDENTIFY_RESPONSE, AMCOM_IdentifyRequestPayload, AMCOM_IdentifyResponsePayload> {
  public:
    IdentifyTransaction() {
        const AMCOM_IdentifyRequestPayload identifyRequestPayload = {0, 2, 0};
        // prepare request data
        AMCOM_Serialize(AMCOM_IDENTIFY_REQUEST, &identifyRequestPayload, sizeof(identifyRequestPayload), requestPayload);
    }

    std::string getName(unsigned int clientId) {
        if (auto r = getResponse(clientId)) {
            return r.value().playerName;
        } else {
            return "???";
        }
    }
};

class NewGameTransaction : public AMCOMTransaction<AMCOM_NEW_GAME_REQUEST, AMCOM_NEW_GAME_RESPONSE, AMCOM_NewGameRequestPayload, AMCOM_NewGameResponsePayload> {
  public:
    NewGameTransaction(uint8_t playerNumber, uint8_t numberOfPlayers) {
        const AMCOM_NewGameRequestPayload newGame = {playerNumber, numberOfPlayers, 1000, 1000};
        // prepare request data
        AMCOM_Serialize(AMCOM_NEW_GAME_REQUEST, &newGame, sizeof(AMCOM_NewGameRequestPayload), requestPayload);
    }

    std::string getHelloMessage(unsigned int clientId) {
        if (auto r = getResponse(clientId)) {
            return r.value().helloMessage;
        } else {
            return "";
        }
    }
};


class FoodUpdateTransaction : public connection::Transaction {
  public:
    FoodUpdateTransaction() : connection::Transaction({}) {
        // reserve max space to avoid relocations
        foodState.reserve(AMCOM_MAX_FOOD_UPDATES);
    }
    virtual ~FoodUpdateTransaction() { ; }

    void addFood(const AMCOM_FoodState& food) { foodState.push_back(food); }

    bool isFull() { return (foodState.size() == AMCOM_MAX_FOOD_UPDATES); }

    bool isEmpty() { return foodState.empty(); }

    void clear() { foodState.clear(); }

    void updateRequest() {
        size_t requestSize = AMCOM_Serialize(AMCOM_FOOD_UPDATE_REQUEST, foodState.data(), foodState.size() * sizeof(AMCOM_FoodState), requestData);
        request            = std::span<const uint8_t>(requestData, requestSize);
    }

  private:
    std::vector<AMCOM_FoodState> foodState;
    uint8_t                      requestData[AMCOM_MAX_PACKET_SIZE];
};


class PlayerUpdateTransaction : public connection::Transaction {
  public:
    PlayerUpdateTransaction() : connection::Transaction({}) {
        // reserve max space to avoid relocations
        playerState.reserve(AMCOM_MAX_PLAYER_UPDATES);
    }
    virtual ~PlayerUpdateTransaction() { ; }

    void addPlayer(const AMCOM_PlayerState& player) { playerState.push_back(player); }

    bool isFull() { return (playerState.size() == AMCOM_MAX_PLAYER_UPDATES); }

    void clear() { playerState.clear(); }

    void updateRequest() {
        size_t requestSize = AMCOM_Serialize(AMCOM_PLAYER_UPDATE_REQUEST, playerState.data(), playerState.size() * sizeof(AMCOM_PlayerState), requestData);
        request            = std::span<const uint8_t>(requestData, requestSize);
    }

  private:
    std::vector<AMCOM_PlayerState> playerState;
    uint8_t                        requestData[AMCOM_MAX_PACKET_SIZE];
};


class MoveTransaction : public AMCOMTransaction<AMCOM_MOVE_REQUEST, AMCOM_MOVE_RESPONSE, AMCOM_MoveRequestPayload, AMCOM_MoveResponsePayload> {
  public:
    MoveTransaction(uint32_t gameTime) {
        const AMCOM_MoveRequestPayload moveRequest = {gameTime};
        // prepare request data
        AMCOM_Serialize(AMCOM_MOVE_REQUEST, &moveRequest, sizeof(AMCOM_MoveRequestPayload), requestPayload);
    }

    float getAngle(unsigned int clientId) {
        if (auto r = getResponse(clientId)) {
            return r.value().angle;
        } else {
            return 0.0;
        }
    }
};


class GameOverTransaction : public AMCOMTransaction<AMCOM_GAME_OVER_REQUEST, AMCOM_GAME_OVER_RESPONSE, AMCOM_GameOverRequestPayload, AMCOM_GameOverResponsePayload> {
  public:
    GameOverTransaction() {
        // reserve max space to avoid relocations
        playerState.reserve(AMCOM_MAX_PLAYER_UPDATES);
    }

    void addPlayer(const AMCOM_PlayerState& player) { playerState.push_back(player); }

    bool isFull() { return (playerState.size() == AMCOM_MAX_PLAYER_UPDATES); }

    void clear() { playerState.clear(); }
    void updateRequest() {
        size_t requestSize = AMCOM_Serialize(AMCOM_GAME_OVER_REQUEST, playerState.data(), playerState.size() * sizeof(AMCOM_PlayerState), requestData);
        request            = std::span<const uint8_t>(requestData, requestSize);
    }

    std::string getEndMessage(unsigned int clientId) {
        if (auto r = getResponse(clientId)) {
            return r.value().endMessage;
        } else {
            return "";
        }
    }

  private:
    std::vector<AMCOM_PlayerState> playerState;
    uint8_t                        requestData[AMCOM_MAX_PACKET_SIZE];
};

#endif /* AMCOM_TRANSACTIONS_H_ */
