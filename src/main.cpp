#include "amgame.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// Application entry point
int main(void) {
    srand(time(NULL));

    // initialize game
    amgame::Game game;

    // wait for at least two clients
    while (1) {
        auto clients = game.server.getClients();
        if (clients.size() >= 2) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };
    // start new match
    game.newMatch();

    // This is the main game loop
    while (1) {
        // get current time in milliseconds using chrono
        auto gameTime = std::chrono::system_clock::now();
        game.update();
        // check player hp
        for (auto& p : game.players) {
            std::cout << "Player " << p->name << ": " << p->lastHp << std::endl;
        }
    }
}
