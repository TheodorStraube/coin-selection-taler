//
// Created by Bohdan Potuzhnyi on 29.04.2024.
//

#ifndef COIN_SELECTION_C_SIMULATION_H
#define COIN_SELECTION_C_SIMULATION_H

#include "common.h"
#include "user.h"
#include "coin_selection.h"

typedef struct {
    clock_t start_time;
    long long accum;


} CPUTimer;
void start(CPUTimer *timer);
void pause_timer(CPUTimer *timer);
float read_timer(CPUTimer *timer);

void simulate_user_actions(int user_index, User user, Wallet denomination_wallet, int num_actions, strategy strategy);
void refresh_dirty_coins(Wallet *wallet, Wallet denomination_wallet, long long time);

#define FALSE  0
#define TRUE  1

#endif //COIN_SELECTION_C_SIMULATION_H
