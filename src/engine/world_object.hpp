#ifndef WORLD_OBJECT_HPP_
#define WORLD_OBJECT_HPP_

#include "box2d.h"

#include <array> // std::array
#include <cmath>



namespace amgame::engine {
    struct Vector2D {
        float x;
        float y;
    };

    class WorldObject {
      public:
        enum Type { BOUNDARIES = 0x1, FOOD = 0x2, PLAYER = 0x4 };
        Type type() const noexcept { return t; };
        WorldObject(Type _) noexcept : t(_) {}

      private:
        Type t;
    };


    /// Object describing box-shaped Map boundaries
    class MapBoundaries : public WorldObject {
      private:
        b2Body*     body{nullptr};
        float const length;

      public:
        MapBoundaries(b2World& world, float sideLength) : WorldObject{WorldObject::Type::BOUNDARIES}, length(sideLength) {
            auto const            l        = length / 2;
            std::array<b2Vec2, 4> vertices = {b2Vec2{-l, -l}, b2Vec2{-l, l}, b2Vec2{l, l}, b2Vec2{l, -l}}; ///< Edges of the map

            b2ChainShape shape{};
            shape.CreateLoop(vertices.data(), vertices.size());

            b2FixtureDef sd{};
            sd.shape       = &shape;
            sd.restitution = 1.0f;
            sd.friction    = 0.0f;

            b2BodyDef bd{};
            bd.userData.pointer = reinterpret_cast<decltype(bd.userData.pointer)>(this);
            body                = world.CreateBody(&bd);
            body->CreateFixture(&sd);
        }

        auto sideLength() const noexcept { return length; }
    };

    /// World object that has hitpoints
    class Mortal {
      protected:
      public:
        constexpr static float minRadius = 25.0f;
        constexpr static float maxRadius = 100.0f;


        int     hitpoints;
        b2Body* body{nullptr};

        Mortal(b2World& world, b2Vec2 p, int initialHP = 1, bool collisionEnabled = true) noexcept : hitpoints{initialHP} {
            b2CircleShape circle{};
            circle.m_radius = hp();

            b2FixtureDef circleShapeDef{};
            circleShapeDef.shape       = &circle;
            circleShapeDef.density     = 1.0f;
            circleShapeDef.friction    = 0.0f;
            circleShapeDef.restitution = 1.0f;
            circleShapeDef.isSensor    = !collisionEnabled;
            b2BodyDef circleBodyDef{};

            circleBodyDef.type = b2_dynamicBody;
            circleBodyDef.position.Set(p.x, p.y);
            circleBodyDef.fixedRotation  = true; // Disable body rotation to ease computation
            circleBodyDef.linearDamping  = 0.0f;
            circleBodyDef.angularDamping = 0.0f;
            body                         = world.CreateBody(&circleBodyDef);
            body->CreateFixture(&circleShapeDef);
        }

        ~Mortal() { body->GetWorld()->DestroyBody(body); }

        /// Returns boolean describing whether the object is alive
        bool alive() const noexcept { return hitpoints > 0; }

        void setPosition(Vector2D pos) { body->SetTransform(b2Vec2(pos.x, pos.y), body->GetAngle()); }

        /**
         * @brief Inflict damage of given magnitude on the object
         *
         * @param damage amount of hitpoints to be deduced
         */

        void harm(int damage) noexcept { hitpoints -= damage; }

        /// Inflict lethal damage on the object
        void kill() noexcept { harm(hitpoints); }

        /**
         * @brief Heal the object with given amount of hitpoints
         *
         * @param healPower amount of hitpoints to be added
         */
        void heal(int healPower) noexcept { hitpoints += healPower; }

        /// returns current hitpoint value of the object
        int hp() const noexcept { return hitpoints; }


        float getRadius() const noexcept { return body->GetFixtureList()->GetShape()->m_radius; }

        /// Update the world object after each world step
        void update() noexcept {
            // Update the radius of the object based on its hitpoints
            body->GetFixtureList()->GetShape()->m_radius = minRadius + hp();
            body->SetEnabled(alive());
        }

        Vector2D getPosition() const {
            auto const p = body->GetPosition();
            return {p.x, p.y};
        }

        [[deprecated("Use getPosition() instead")]] Vector2D position() const {
            auto const p = body->GetPosition();
            return {p.x, p.y};
        }
    };

    /// World object that acts as Food
    class Food : public WorldObject, public Mortal {
      public:
        Food(b2World& world, b2Vec2 p) : WorldObject{WorldObject::Type::FOOD}, Mortal(world, p, 1, false) { body->GetUserData().pointer = reinterpret_cast<decltype(body->GetUserData().pointer)>(this); }
    };


    /// World object that acts as Player
    class Player : public WorldObject, public Mortal {
      public:
        Player(b2World& world, b2Vec2 p) : WorldObject{WorldObject::Type::PLAYER}, Mortal(world, p, 2, true) { body->GetUserData().pointer = reinterpret_cast<decltype(body->GetUserData().pointer)>(this); }

        void setAngle(float phi) {
            // body->ApplyLinearImpulseToCenter(b2Vec2{cos(phi), sin(phi)}, true);
            body->SetLinearVelocity(b2Vec2{10 * cos(phi), 10 * sin(phi)});
        }
        float getAngle() const noexcept {
            auto const v = body->GetLinearVelocity();
            return atan2(v.y, v.x);
        }
    };

} // namespace amgame::engine
#endif