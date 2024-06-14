#ifndef AMGAME_PLAYER_H_
#define AMGAME_PLAYER_H_
#pragma once

#include "engine.hpp"
#include "remote_connection.h"


namespace amgame {
    class Game;

    class Player {
      public:
        engine::Player& enginePlayer; ///< player representation in physics engine
        int             lastHp;
        char            hpText[10];
        unsigned int    clientId; ///< remote client id
        std::string     name;
        Player(Game& game, engine::World& world, std::string name, std::string description, std::string helloMessage, unsigned int clientId);
        ~Player();

        void updateSprite();
        void hideMessage();

      private:
        Game& game;
    };

} // namespace amgame

#endif /* AMGAME_PLAYER_H_ */
