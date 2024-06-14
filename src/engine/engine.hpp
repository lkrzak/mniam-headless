#ifndef ENGINE_HPP_
#define ENGINE_HPP_
#pragma once

#include "box2d.h"
#include "world_object.hpp"

#include <list>   // std::list
#include <memory> // std::unique_ptr
#include <random> // std::random_device


namespace amgame::engine {
    constexpr static bool verbose{false};


    class ContactListener : public b2ContactListener {
      protected:
        void BeginContact(b2Contact* contact) override;
        void EndContact([[maybe_unused]] b2Contact* contact) override;
        void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override;

      public:
        template<class T> static void handle(T& o1, WorldObject& o2) noexcept;

        static void handle(MapBoundaries& o1, MapBoundaries& o2) noexcept;

        static void handle(MapBoundaries& o1, Food& o2) noexcept;

        static void handle(MapBoundaries& o1, Player& o2) noexcept;

        static void handle(Food& o1, Food& o2) noexcept;

        static void handle(Food& o1, Player& o2) noexcept;

        static void handle(Player& o1, Player& o2) noexcept;

        static void handle(Food& o1, MapBoundaries& o2) noexcept {
            handle(o2, o1);
        }
        static void handle(Player& o1, MapBoundaries& o2) noexcept {
            handle(o2, o1);
        }
        static void handle(Player& o1, Food& o2) noexcept {
            handle(o2, o1);
        }
    };


    class World {
      private:
        b2Vec2 const                    gravity{0.0f, 0.0f};
        constexpr static int32_t        velocityIterations{8};
        constexpr static int32_t        positionIterations{3};
        constexpr static float          timeStep{900.0f / (600.0f)};
        std::mt19937                    gen; // seed the generator
        std::uniform_int_distribution<> distr;

        void processOngioingContacts() noexcept;

      public:
        b2World                            world{gravity};
        std::list<std::unique_ptr<Food>>   food{};
        std::list<std::unique_ptr<Player>> players{};
        MapBoundaries                      boundaries;
        ContactListener                    contactListener;


      public:
        World(float size) noexcept : boundaries(world, size), gen(std::random_device()()), distr(-size / 2 + Mortal::minRadius, size / 2 - Mortal::minRadius) {
            world.SetContactListener(&contactListener);
        }


        Food& addFood(b2Vec2 p) {
            food.push_back(std::make_unique<Food>(world, p));
            return *food.back();
        }
        Food& addFood() {
            return addFood(b2Vec2{float(distr(gen)), float(distr(gen))});
        };

        Player& addPlayer(b2Vec2 p) {
            players.push_back(std::make_unique<Player>(world, p));
            return *players.back();
        }
        Player& addPlayer() {
            return addPlayer(b2Vec2{float(distr(gen)), float(distr(gen))});
        }

        void init() noexcept;
        void step() noexcept;
    };
} // namespace amgame::engine
#endif
