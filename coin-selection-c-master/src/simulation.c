//
// Created by Bohdan Potuzhnyi on 29.04.2024.
//
#include "simulation.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/**
 * @brief Get the scale of a given amount.
 *
 * @param amount The amount to determine the scale for.
 * @return The scale of the amount.
 */
int get_scale(long long amount) {
    int scale = 0;
    while (amount > 0) {
        amount /= 10;
        scale++;
    }
    return scale;
}

/**
 * @brief Simulate user actions and write results to a file.
 *
 * @param user_index The index of the user.
 * @param user The user structure containing user information.
 * @param denomination_wallet The wallet containing denominations.
 * @param num_actions The number of actions to simulate.
 * @param strategy The strategy to use for coin allocation.
 */
void simulate_user_actions(int user_index, User user, Wallet denomination_wallet, int num_actions, strategy strategy) {
    const char* TypeNames[] = {"STUDENT", "STUDENT_STATIC","BUSINESS_OWNER", "RETIRED", "FAMILY", "FREELANCER", "TEACHER", "ARTIST"};
    const char* StrategyNames[] = {"MAX_BILLS", "MIN_BILLS", "CLOSEST_TO_EXPIRE_MIN_BILLS", "CLOSEST_TO_EXPIRE_MAX_BILLS", "MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED", "RANDOM", "EVEN_FROM_MIN_TO_MAX", "EVEN_FROM_MAX_TO_MIN", "GREEDY_MIN_TO_MAX"};
    const char* OperationNames[] = {"DEPOSIT_OP", "WITHDRAW_OP", "REFUND_OP", "REFRESH_OP", "WIRE_OP", "CLOSE_OP"};

    const int generation_scale = 7;
    long long wallet_scale = 0;

    for(int i = 0; i < denomination_wallet.num_coins; i++){
        if(wallet_scale < denomination_wallet.coins[i].denomination.amount)
            wallet_scale = denomination_wallet.coins[i].denomination.amount;
    }

    wallet_scale = get_scale(wallet_scale);

    char filename[256];
    sprintf(filename, "../simulation/results/user_%d_strategy_%s.csv", user_index, StrategyNames[strategy]);

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Failed to open file %s for writing.\n", filename);
    }

    user.wallet.num_coins = 0;

    long long total_fee = 0;
    if (user.actions != NULL && num_actions > 0) {
        fprintf(fp, "%s, %s\n", TypeNames[user.type], StrategyNames[strategy]);

        for (int i = 0; i < num_actions; i++) {
            long long renew_fee = calculate_renew_fee(user.wallet, user.actions[i].time);
            if (renew_fee > 0) {
                fprintf(fp, "%d_%d, %lld, %lld, %s, %lld\n", user_index, i, user.actions[i].time, 0ll, OperationNames[REFRESH_OP], renew_fee);
                total_fee += renew_fee;
            }

            long long fee_for_action;
            long long fee_for_change;


            long long transaction_amount = user.actions[i].amount;

            if(wallet_scale > generation_scale){
                transaction_amount = pow(10, wallet_scale - generation_scale) * transaction_amount;
            } else if(wallet_scale < generation_scale){
                transaction_amount = floor(transaction_amount / pow(10, generation_scale - wallet_scale));
            }

            if (user.actions[i].operation == WITHDRAW_OP || user.actions[i].operation == REFUND_OP) {
                // Simulate deposit or refund
                int generatedCoinCount = 0;
                Coin *generatedCoins = generate_withdraw_coins(transaction_amount,
                                                               user.actions[i].time, denomination_wallet,
                                                               &generatedCoinCount);

                if (generatedCoins) {
                    fee_for_action = calculate_total_fee(generatedCoins, generatedCoinCount,
                                                         user.actions[i].operation);

                    fprintf(fp, "%d_%d, %lld, %lld, %s, %lld\n", user_index, i, user.actions[i].time, transaction_amount,
                            OperationNames[user.actions[i].operation], fee_for_action);

                    total_fee += fee_for_action;
                    add_coins_to_wallet(&user.wallet, generatedCoins, generatedCoinCount);
                }
            } else if (user.actions[i].operation == DEPOSIT_OP) {
                // Simulate withdrawal
                int allocatedCoinCount = 0;
                long long allocatedAmount = 0;
                Coin *allocatedCoins = allocate_coins_for_deposit(user.wallet, transaction_amount,
                                                                  strategy,
                                                                  user.actions[i].time, &allocatedCoinCount,
                                                                  &allocatedAmount, denomination_wallet);

                if (allocatedCoins) {
                    fee_for_action = calculate_total_fee(allocatedCoins, allocatedCoinCount, DEPOSIT_OP);

                    fprintf(fp, "%d_%d, %lld, %lld, %s, %lld\n", user_index, i, user.actions[i].time, transaction_amount,
                            OperationNames[user.actions[i].operation], fee_for_action);

                    remove_selected_coins(&user.wallet, allocatedCoins, allocatedCoinCount);
                    long long changeAmount = allocatedAmount - transaction_amount; // Assuming fees are subtracted from the withdrawn amount

                    if (changeAmount > 0) {
                        int changeCoinCount = 0;
                        Coin *changeCoins = generate_withdraw_coins(changeAmount,
                                                                    user.actions[i].time,
                                                                    denomination_wallet, &changeCoinCount);
                        fee_for_change = calculate_total_fee(changeCoins, changeCoinCount, REFRESH_OP);

                        fprintf(fp, "%d_%d, %lld, %lld, %s, %lld\n", user_index, i, user.actions[i].time, transaction_amount,
                               OperationNames[REFRESH_OP], fee_for_change);

                        fprintf(fp, "%d_%d, %lld, %lld, DEPOSIT_REFRESH_OP, %lld\n", user_index, i, user.actions[i].time,
                                transaction_amount, fee_for_action + fee_for_change);

                        if (changeCoins) {
                            add_coins_to_wallet(&user.wallet, changeCoins, changeCoinCount);
                        } else {
                            printf("Error for allocation of change coins\n");
                        }
                    } else {
                        fprintf(fp, "%d_%d, %lld, %lld, DEPOSIT_REFRESH_OP, %lld\n", user_index, i, user.actions[i].time,
                                transaction_amount, fee_for_action);
                    }
                    total_fee += fee_for_action;
                } else {
                    printf("Error for allocation of coins\n");
                }
            }
        }
    } else {
        printf("No actions generated for the user.\n");
    }

    fclose(fp);


    free(user.wallet.coins);
    free(user.actions);
}
