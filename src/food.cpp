#include "food.h"

namespace amgame {

    Food::Food(engine::World& world) : engineFood(world.addFood()) {
        isAlive = true;
    }

    Food::~Food() {
        ;
    }


    bool Food::updateSprite() {
        if (isAlive) {
            if (!engineFood.alive()) {
                isAlive = false;
                return true;
            }
        }
        return false;
    }

} // namespace amgame
