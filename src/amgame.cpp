#include "amgame.h"

#include "food.h"
#include "player.h"

#include <syncstream>


namespace amgame {

    Game::Game() : numberOfPlayers(), mapWidth(1000), mapHeight(1000), phase(MAIN_MENU), world(1000.0) {}

    Game::~Game() {
        clear();
    }

    void Game::clear() {
        for (auto& p : players) {
            delete p;
        }
        players.clear();
        food.clear();
    }

    void Game::newMatch() {
        // remove players with dead connection
        server.removeAllInactiveClients();
        // get number of players
        numberOfPlayers = server.getClients().size();
        std::osyncstream(std::cout) << "New match with " << (int)numberOfPlayers << " players" << std::endl;

        this->mapWidth  = 1000.0;
        this->mapHeight = 1000.0;

        phase = NEW_GAME_REQUEST;
    }

    void Game::addBot() {
        std::osyncstream(std::cout) << "Adding bots is not supported in headless" << std::endl;
    }

    void Game::positionPlayers() {
        float angleInc = (2 * 3.1416) / players.size();
        float angle    = 0.0;
        for (auto& p : players) {
            float x = 250 * cos(angle);
            float y = 250 * sin(angle);
            p->enginePlayer.setPosition({x, y});
            p->enginePlayer.setAngle(0.0);
            p->updateSprite();
            angle += angleInc;
        }
    }

    void Game::positionFood() {
        for (size_t f = 0; f < players.size() * 6; f++) {
            food.emplace_back(world);
        }
    }

    void Game::update() {
        switch (phase) {
            case NEW_GAME_REQUEST: {
                // send NEW_GAME.request to all players individually and get responses
                auto    clients  = server.getClients();
                uint8_t playerNo = 0;
                for (auto& client : clients) {
                    auto newGameTransaction = NewGameTransaction(playerNo, clients.size());
                    server.runTransactionWithSingleClient(client.clientId, newGameTransaction);
                    if (1 == newGameTransaction.waitForFinish(std::chrono::milliseconds(500))) {
                        // add new player
                        auto p = new Player(*this, world, identifyTransaction.getName(client.clientId), "online", newGameTransaction.getHelloMessage(client.clientId), client.clientId);
                        players.push_back(p);
                    }
                    playerNo++;
                }
                // position players on the screen
                positionPlayers();
                // position also food
                positionFood();
                world.init();
                for (auto& p : players) {
                    p->updateSprite();
                }
                for (auto& f : food) {
                    f.updateSprite();
                }
                // move to next game phase
                phase = FOOD_UPDATE_REQUEST;
            } break;
            case FOOD_UPDATE_REQUEST: {
                uint16_t              foodNo = 0;
                FoodUpdateTransaction foodUpdateTransaction;
                for (auto& f : food) {
                    // prepare food state
                    AMCOM_FoodState foodState;
                    foodState.foodNo = foodNo;
                    foodState.state  = f.engineFood.hp();
                    foodState.x      = f.engineFood.getPosition().x;
                    foodState.y      = f.engineFood.getPosition().y;
                    // add it to transaction
                    foodUpdateTransaction.addFood(foodState);

                    // if we reached the number of food states per transaction or this is the last food in the queue
                    if ((foodUpdateTransaction.isFull()) || (&f == &food.back())) {
                        foodUpdateTransaction.updateRequest();
                        server.runTransaction(foodUpdateTransaction);
                        foodUpdateTransaction.waitForFinish(std::chrono::milliseconds(100));
                        foodUpdateTransaction.clear();
                    }
                    foodNo++;
                }
                // move to next game phase
                phase = PLAYER_UPDATE_REQUEST;
            } break;
            case PLAYER_UPDATE_REQUEST: {
                uint16_t                playerNo = 0;
                PlayerUpdateTransaction playerUpdateTransaction;
                for (const auto& p : players) {
                    // prepare player state
                    AMCOM_PlayerState playerState;
                    playerState.playerNo = playerNo;
                    playerState.hp       = p->enginePlayer.hp();
                    playerState.x        = p->enginePlayer.getPosition().x;
                    playerState.y        = p->enginePlayer.getPosition().y;
                    // add it to transaction
                    playerUpdateTransaction.addPlayer(playerState);

                    // if we reached the number of food states per transaction or this is the last food in the queue
                    if ((playerUpdateTransaction.isFull()) || (&p == &players.back())) {
                        playerUpdateTransaction.updateRequest();
                        server.runTransaction(playerUpdateTransaction);
                        playerUpdateTransaction.waitForFinish(std::chrono::milliseconds(100));
                        playerUpdateTransaction.clear();
                    }
                    playerNo++;
                }
                // move to next game phase
                phase = MOVE_REQUEST;
            } break;
            case MOVE_REQUEST: {
                // move to next game phase
                static uint32_t gameTime;
                MoveTransaction moveTransaction(gameTime++);
                server.runTransaction(moveTransaction);
                moveTransaction.waitForFinish(std::chrono::milliseconds(500));
                for (const auto& p : players) {
                    p->enginePlayer.setAngle(moveTransaction.getAngle(p->clientId));
                }
                world.step();
                world.step();
                world.step();
                world.step();
                world.step();
                for (auto& p : players) {
                    p->updateSprite();
                }
                uint16_t              foodNo = 0;
                FoodUpdateTransaction foodUpdateTransaction;
                for (auto& f : food) {
                    if (f.updateSprite()) {
                        // prepare food state
                        AMCOM_FoodState foodState;
                        foodState.foodNo = foodNo;
                        foodState.state  = 0;
                        foodState.x      = f.engineFood.getPosition().x;
                        foodState.y      = f.engineFood.getPosition().y;
                        // add it to transaction
                        foodUpdateTransaction.addFood(foodState);
                    }
                    // if we reached the number of food states per transaction or this is the last food in the queue and we have something to send
                    if ((foodUpdateTransaction.isFull()) || ((&f == &food.back()) && (!foodUpdateTransaction.isEmpty()))) {
                        foodUpdateTransaction.updateRequest();
                        server.runTransaction(foodUpdateTransaction);
                        foodUpdateTransaction.waitForFinish(std::chrono::milliseconds(100));
                        foodUpdateTransaction.clear();
                    }
                    foodNo++;
                }
                phase = PLAYER_UPDATE_REQUEST;
            } break;
            case GAME_OVER_REQUEST: {
                uint16_t            playerNo = 0;
                GameOverTransaction gameOverTransaction;
                for (const auto& p : players) {
                    // prepare food state
                    AMCOM_PlayerState playerState;
                    playerState.playerNo = playerNo;
                    playerState.hp       = p->enginePlayer.hp();
                    playerState.x        = p->enginePlayer.getPosition().x;
                    playerState.y        = p->enginePlayer.getPosition().y;
                    // add it to transaction
                    gameOverTransaction.addPlayer(playerState);

                    // if we reached the number of food states per transaction or this is the last food in the queue
                    if ((gameOverTransaction.isFull()) || (&p == &players.back())) {
                        gameOverTransaction.updateRequest();
                        server.runTransaction(gameOverTransaction);
                        gameOverTransaction.waitForFinish(std::chrono::milliseconds(100));
                        gameOverTransaction.clear();
                    }
                    playerNo++;
                }
                phase = GAME_END;
            } break;
            case GAME_END: break;
        } // switch (phase)
    }

    void Game::finish() {
        phase = GAME_OVER_REQUEST;
    }

} // namespace amgame
