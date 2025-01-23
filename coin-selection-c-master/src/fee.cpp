//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 04.03.2024.
// fee.c
//

#include "fee.h"

/**
 * @brief Calculate the fee for a given coin and operation type.
 *
 * @param coin The coin for which the fee is being calculated.
 * @param operation The type of operation (e.g., DEPOSIT_OP, WITHDRAW_OP, REFUND_OP, REFRESH_OP).
 * @return The total fee in satoshis for the specified operation.
 */
long long calculate_fee(Coin coin, operation_type operation) {
    Fee current_fee;

    // Determine the appropriate fee based on the operation type
    switch (operation) {
    case DEPOSIT_OP:
        current_fee = coin.denomination.rules.fees.deposit_fee;
        break;
    case WITHDRAW_OP:
        current_fee = coin.denomination.rules.fees.withdraw_fee;
        break;
    case REFUND_OP:
        current_fee = coin.denomination.rules.fees.refund_fee;
        break;
    case REFRESH_OP:
        current_fee = coin.denomination.rules.fees.refresh_fee;
        break;
    default:
        current_fee = coin.denomination.rules.fees.deposit_fee;
        break;
    }

    long long total_fee = current_fee.fee_satoshis;
    return total_fee;
}

