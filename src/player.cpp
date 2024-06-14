#include "player.h"

#include "amgame.h"

namespace amgame {


    static int playerCount;

    Player::Player(Game& game, engine::World& world, std::string name, std::string description, std::string helloMessage, unsigned int clientId) :
        game(game), enginePlayer(world.addPlayer()), clientId(clientId), name(name) {
        lastHp = enginePlayer.hp();
        playerCount++;
    }

    Player::~Player() {
        playerCount--;
    }

    void Player::updateSprite() {
        if (enginePlayer.alive()) {
            int radius = (int)enginePlayer.getRadius();
            lastHp     = enginePlayer.hp();
        }
    }

} // namespace amgame
