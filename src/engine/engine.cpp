#include "engine.hpp"

#include <iostream>


namespace amgame::engine {
    template<class T> void ContactListener::handle(T& o1, WorldObject& o2) noexcept {
        switch (o2.type()) {
            case WorldObject::Type::BOUNDARIES: handle(o1, static_cast<MapBoundaries&>(o2)); break;
            case WorldObject::Type::FOOD: handle(o1, static_cast<Food&>(o2)); break;
            case WorldObject::Type::PLAYER: handle(o1, static_cast<Player&>(o2)); break;
        }
    }

    void ContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold) {
        auto o1 = reinterpret_cast<WorldObject*>(contact->GetFixtureA()->GetBody()->GetUserData().pointer);
        auto o2 = reinterpret_cast<WorldObject*>(contact->GetFixtureB()->GetBody()->GetUserData().pointer);


        if (o1->type() != WorldObject::Type::BOUNDARIES && o2->type() != WorldObject::Type::BOUNDARIES) {
            contact->SetEnabled(false);
        }
    }

    void ContactListener::BeginContact(b2Contact* contact) {
        auto o1 = reinterpret_cast<WorldObject*>(contact->GetFixtureA()->GetBody()->GetUserData().pointer);
        auto o2 = reinterpret_cast<WorldObject*>(contact->GetFixtureB()->GetBody()->GetUserData().pointer);

        if (o1 && o2) {
            if (verbose) {
                std::cout << "C " << o1->type() << ' ' << o2->type() << "\n";
            }

            switch (o1->type()) {
                case WorldObject::Type::BOUNDARIES: handle(static_cast<MapBoundaries&>(*o1), *o2); break;
                case WorldObject::Type::FOOD: handle(static_cast<Food&>(*o1), *o2); break;
                case WorldObject::Type::PLAYER: handle(static_cast<Player&>(*o1), *o2); break;
            }
        }
    }

    void ContactListener::EndContact([[maybe_unused]] b2Contact* contact) {
        if (verbose) {
            std::cout << "Contact End\n";
        }
    }


    void ContactListener::handle(MapBoundaries& o1, MapBoundaries& o2) noexcept {
        if (verbose) {
            std::cout << "Map - Map\n";
        }
    }
    void ContactListener::handle(MapBoundaries& o1, Food& o2) noexcept {
        if (verbose) {
            std::cout << "Map - Food\n";
        }
        o2.kill(); // kill food in case it would like to escape the box... Unlikely,
                   // since food shouldn't move
    }

    void ContactListener::handle(MapBoundaries& o1, Player& o2) noexcept {
        if (verbose) {
            std::cout << "Map - Player\n";
        }
    }


    void ContactListener::handle(Food& o1, Food& o2) noexcept {
        if (verbose) {
            std::cout << "Food - Food\n";
        }
    }

    void ContactListener::handle(Food& o1, Player& o2) noexcept {
        if (verbose) {
            std::cout << "Food - Player\n";
        }
        if (o1.alive() && o2.alive()) {
            o2.heal(o1.hp());
            o1.kill();
        }
    }


    void ContactListener::handle(Player& o1, Player& o2) noexcept {
        if (verbose) {
            std::cout << "Player - Player\n";
        }
        if (o1.alive() && o2.alive()) {
            if (o1.hp() > o2.hp()) {
                o1.heal(o2.hp());
                o2.kill();
            } else if (o2.hp() > o1.hp()) {
                o2.heal(o1.hp());
                o1.kill();
            } else {
                // do nothing
            }
        }
    }

    void World::init() noexcept {
        for (auto& player : players) {
            player->update();
        }
        for (auto& f : food) {
            f->update();
        }
    }

    void World::step() noexcept {
        world.Step(timeStep, velocityIterations, positionIterations);
        processOngioingContacts();
        for (auto& player : players) {
            player->update();
        }
        for (auto& f : food) {
            f->update();
        }
    }


    void World::processOngioingContacts() noexcept {
        for (auto contact = world.GetContactList(); contact; contact = contact->GetNext()) {
            if (contact->IsTouching()) {
                auto o1 = reinterpret_cast<WorldObject*>(contact->GetFixtureA()->GetBody()->GetUserData().pointer);
                auto o2 = reinterpret_cast<WorldObject*>(contact->GetFixtureB()->GetBody()->GetUserData().pointer);
                if (o1 && o2) {
                    if (verbose) {
                        std::cout << "C " << o1->type() << ' ' << o2->type() << "\n";
                    }
                    if (WorldObject::Type::PLAYER == o1->type() && WorldObject::Type::PLAYER == o2->type()) {
                        ContactListener::handle(static_cast<Player&>(*o1), static_cast<Player&>(*o2));
                    }
                }
            }
        }
    }

} // namespace amgame::engine
