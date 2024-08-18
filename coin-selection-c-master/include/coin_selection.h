//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 04.03.2024.
// coin_selection.h
//

#ifndef COIN_SELECTION_H
#define COIN_SELECTION_H

#include "common.h"
#include "fee.h"

typedef enum {
    MAX_BILLS,
    MIN_BILLS,
    CLOSEST_TO_EXPIRE_MIN_BILLS,
    CLOSEST_TO_EXPIRE_MAX_BILLS,
    MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED,
    RANDOM,
    EVEN_FROM_MIN_TO_MAX,
    EVEN_FROM_MAX_TO_MIN,
    GREEDY_MIN_TO_MAX,
    NUMBER_OF_STRATEGIES
} strategy;

typedef struct {
    Coin *coin;
    double score;
} coin_wrapper;

Coin* allocate_coins_for_deposit(Wallet wallet, long long amount, strategy strategy, long long time, int *num_allocated_coins, long long *allocated_amount, Wallet denomination_wallet);

void remove_selected_coins(Wallet* wallet, Coin* coins, int num_coins);

void add_coins_to_wallet(Wallet* wallet, Coin* coins, int num_coins);

long long calculate_total_fee(Coin* coins, int num_coins, operation_type operation);

long long calculate_renew_fee(Wallet wallet, long long time);

Coin* generate_withdraw_coins(long long amount, long long time, Wallet default_wallet, int *num_coins);


#endif //COIN_SELECTION_H
