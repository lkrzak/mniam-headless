#ifndef AMGAME_H_
#define AMGAME_H_

#include "amcom_transactions.h"
#include "connection_server.h"
#include "engine.hpp"
#include "food.h"
#include "player.h"
#include "remote_connection.h"

#include <list>
#include <vector>

namespace amgame {

    class Game {
        friend class Player;

      private:
        /// phase of the game
        enum { MAIN_MENU, TESTER, GAME_IDLE, NEW_GAME_REQUEST, PLAYER_UPDATE_REQUEST, FOOD_UPDATE_REQUEST, MOVE_REQUEST, GAME_OVER_REQUEST, GAME_END } phase;

        size_t        numberOfPlayers;
        float         mapWidth;
        float         mapHeight;
        engine::World world;

        size_t countFinishedTransactions();
        void   positionPlayers();
        void   positionFood();

      public:
        /// List of players
        std::deque<amgame::Player*> players;
        /// List of food
        std::deque<Food> food;
        /// Connection server
        connection::Server server{2001, 8};
        // Identify transaction
        IdentifyTransaction identifyTransaction;
        Game();
        ~Game();
        void addBot();
        void clear();
        void newMatch();
        void update();
        void finish();
    };


} // namespace amgame

#endif /* AMGAME_H_ */
