//
// Created by Bohdan Potuzhnyi on 29.04.2024.
//

#ifndef COIN_SELECTION_C_SIMULATION_H
#define COIN_SELECTION_C_SIMULATION_H

#include "common.h"
#include "fee.h"
#include "user.h"
#include "coin_selection.h"

void simulate_user_actions(int user_index, User user, Wallet denomination_wallet, int num_actions, strategy strategy);

#endif //COIN_SELECTION_C_SIMULATION_H
