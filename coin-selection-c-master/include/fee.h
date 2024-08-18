//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 04.03.2024.
// fee.h
//

#ifndef FEE_H
#define FEE_H

#include "common.h" // Include the common definitions

typedef enum {
    DEPOSIT_OP,
    WITHDRAW_OP,
    REFUND_OP,
    REFRESH_OP,
    WIRE_OP,
    CLOSE_OP
} operation_type;

typedef struct{
    long long amount;
    operation_type operation;
    long long time;
} Action;

long long calculate_fee(Coin coin, operation_type operation);

#endif // FEE_H

