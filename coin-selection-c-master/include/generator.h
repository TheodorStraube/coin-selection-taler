//
// Created by Bohdan Potuzhnyi on 24.03.2024.
//

#ifndef COIN_SELECTION_C_GENERATOR_H
#define COIN_SELECTION_C_GENERATOR_H

#include "user.h"
#include "fee.h"


long long rand_range_long(long long min, long long max);
void generate_actions_for_user(User user, Action **actions, int *size);
void generate_and_save_actions(const char *base_dir, int num_users);

#endif //COIN_SELECTION_C_GENERATOR_H
