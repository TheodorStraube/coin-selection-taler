//
// Created by Bohdan Potuzhnyi on 29.04.2024.
//

#ifndef COIN_SELECTION_C_SIMULATION_H
#define COIN_SELECTION_C_SIMULATION_H

#include "common.h"
#include "user.h"
#include "coin_selection.h"

void simulate_user_actions(int user_index, User user, Wallet denomination_wallet, int num_actions, strategy strategy);
void refresh_dirty_coins(Wallet *wallet, Coin *coins, int num_coins, Wallet denomination_wallet, long long time);

#endif //COIN_SELECTION_C_SIMULATION_H
