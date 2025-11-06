//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 04.03.2024.
// common.h
//

#ifndef COMMON_H
#define COMMON_H

#include <time.h>

typedef struct {
    long long fee_satoshis;
    float percentage_fee;
} Fee;

typedef struct {
    Fee deposit_fee;
    Fee refund_fee;
    Fee withdraw_fee;
    Fee refresh_fee;
} Fees;

typedef struct{
    Fee wire_fee;
    Fee closing_fee;
} GlobalFees;

typedef struct {
    long long time;
} Duration;

typedef struct {
    Duration legal;
    Duration deposit;
    Duration withdraw;
} Durations;

typedef struct {
    int rsa_keysize;
    char* cipher;
    Fees fees;
    Durations durations;
} Rules;

typedef struct{
    char* name;
    long long amount;
    Rules rules;
} Denomination;

typedef struct {
    long long uniqueId;
    Denomination denomination;
    time_t creation_timestamp;
    long long amount;
    long latest_recoup_time;
} Coin;

typedef struct {
    Coin* coins;
    int num_coins;
    GlobalFees global_fees;
} Wallet;

typedef struct {
    const char* filepath;
    Wallet denomination_wallet;
    int set_strategy;
} ThreadArgs;

typedef struct {
    long long amount;
    Coin coin;
} PartialCoin; 

typedef struct {
    long long instructed_amount;
    long long effective_amount;

    long long deposit_fee_sum;
    long long refresh_fee_sum;

    int valid;
} FeeTab;

typedef struct {
    Coin* coins;
    int coin_count;
    FeeTab tab;
} CoinSelectionResult;

#define FALSE  0
#define TRUE  1

#endif //COMMON_H
