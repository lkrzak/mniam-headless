#ifndef AMGAME_FOOD_H_
#define AMGAME_FOOD_H_

#include "engine.hpp"

namespace amgame {

    class Food {
      public:
        engine::Food& engineFood; ///< food representation in physics engine
        bool          isAlive;
        Food(engine::World& world);
        ~Food();
        bool updateSprite();

      private:
    };

} // namespace amgame

#endif /* AMGAME_FOOD_H_ */
