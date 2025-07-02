//
// Created by Bohdan Potuzhnyi on 29.04.2024.
//
#include "simulation.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "cjson/cJSON.h"
#include "coin_selection.h"
#include "common.h"
#include "simulation.h"
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

long long coins_balance(Coin* coins, int num_coins) {
    long long total = 0;
      for (int i = 0; i < num_coins; ++i) {
        total += coins[i].amount;
      }
      return total;
}

void print_deposit_status(int i, long long transaction_amount, Wallet* wallet, CoinSelectionResult* allocatedCoins){
    long long wallet_balance = coins_balance(wallet->coins, wallet->num_coins); 
    long long alloc_balance = coins_balance(allocatedCoins->coins, allocatedCoins->coin_count);

    //assert(alloc_balance <= wallet_balance);
    //assert(alloc_balance >= transaction_amount);

    // printf("[%d]\tAmt: %lld, Wallet[%lld]: %lld\t\tALLOC[%d]: [%lld\t%lld]\n",i, transaction_amount, wallet->num_coins, wallet_balance, allocatedCoins->coin_count, alloc_balance, allocatedCoins->tab.effective_amount);
}

void add_coins_to_array(Wallet wallet, cJSON *json) {

    for(int i = 0; i < wallet.num_coins; i++) {
        cJSON *coin_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(coin_json, "value", wallet.coins[i].amount);
        cJSON_AddItemToArray(json, coin_json);
    }
    
}

void save_wallet_state(Wallet wallet, long long time, FILE *file_handle) {
    cJSON *json = cJSON_CreateObject();; 
    cJSON_AddNumberToObject(json, "time", time);
    
    cJSON *coins = cJSON_AddArrayToObject(json, "coins");
    add_coins_to_array(wallet, coins);

    char *json_str = cJSON_PrintUnformatted(json);
    fputs(json_str, file_handle);
    fprintf(file_handle, ",\n");

    cJSON_Delete(json);
    free(json_str);
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
void simulate_user_actions(int user_index, User user,
                           Wallet denomination_wallet, int num_actions,
                           strategy strategy) {
  const char *TypeNames[] = {"STUDENT", "STUDENT_STATIC", "BUSINESS_OWNER",
                             "RETIRED", "FAMILY",         "FREELANCER",
                             "TEACHER", "ARTIST"};
  const char *StrategyNames[] = {"MAX_BILLS",
                                 "MIN_BILLS",
                                 "CLOSEST_TO_EXPIRE_MIN_BILLS",
                                 "CLOSEST_TO_EXPIRE_MAX_BILLS",
                                 "MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED",
                                 "RANDOM",
                                 "EVEN_FROM_MIN_TO_MAX",
                                 "EVEN_FROM_MAX_TO_MIN",
                                 "GREEDY_MIN_TO_MAX",
                                 "GREEDY_MIN_TO_MAX_FIX",
                                 "CALL_EXTERNAL",
                                 "WALLET_CORE"};
  const char *OperationNames[] = {"DEPOSIT_OP", "WITHDRAW_OP", "REFUND_OP",
                                  "REFRESH_OP", "WIRE_OP",     "CLOSE_OP"};
  const int generation_scale = 7;
  long long wallet_scale = 0;

  for (int i = 0; i < denomination_wallet.num_coins; i++) {
    if (wallet_scale < denomination_wallet.coins[i].denomination.amount)
      wallet_scale = denomination_wallet.coins[i].denomination.amount;
  }

  wallet_scale = get_scale(wallet_scale);

  char logfilename[256];
  sprintf(logfilename, "../simulation/results/log_user_%d_strategy_%s.json", user_index, StrategyNames[strategy]);

  FILE *log_fp = fopen(logfilename, "w");

  char filename[256];
  sprintf(filename, "../simulation/results/user_%d_strategy_%s.csv", user_index,
          StrategyNames[strategy]);

  FILE *fp = fopen(filename, "w");
  if (!fp) {
    printf("Failed to open file %s for writing.\n", filename);
  }

  user.wallet.num_coins = 0;

  long long total_fee = 0;

  if (user.actions != NULL && num_actions > 0) {
    fprintf(fp, "%s, %s\n", TypeNames[user.type], StrategyNames[strategy]);

        // printf("\n");
    // printf("%d Aktionen\n", num_actions);

    fprintf(log_fp, "[");
    save_wallet_state(user.wallet, -1, log_fp);

    for (int i = 0; i < num_actions; i++) {

        // printf("\n");
        printf("[%s][%lld]\t%s [%lld]\n", StrategyNames[strategy], user.actions[i].time, OperationNames[user.actions[i].operation], user.actions[i].amount);

    

      long long renew_fee =
          calculate_renew_fee(user.wallet, user.actions[i].time);
      if (renew_fee > 0) {
        fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d\n", user_index, i,
                user.actions[i].time, 0ll, OperationNames[REFRESH_OP],
                renew_fee, user.wallet.num_coins);
        total_fee += renew_fee;
      }

      long long fee_for_action;

      long long transaction_amount = user.actions[i].amount;

      if (wallet_scale > generation_scale) {
        transaction_amount =
            pow(10, wallet_scale - generation_scale) * transaction_amount;
      } else if (wallet_scale < generation_scale) {
        transaction_amount = floor(transaction_amount /
                                   pow(10, generation_scale - wallet_scale));
      }

      if (user.actions[i].operation == WITHDRAW_OP ||
          user.actions[i].operation == REFUND_OP) {
        // Simulate deposit or refund
        int generatedCoinCount = 0;
        Coin *generatedCoins =
            generate_withdraw_coins(transaction_amount, user.actions[i].time,
                                    denomination_wallet, &generatedCoinCount);
        if (generatedCoins) {
          fee_for_action = calculate_total_fee(
              generatedCoins, generatedCoinCount, user.actions[i].operation);

          fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d\n", user_index, i,
                  user.actions[i].time, transaction_amount,
                  OperationNames[user.actions[i].operation], fee_for_action,
                  user.wallet.num_coins);

          total_fee += fee_for_action;
          add_coins_to_wallet(&user.wallet, generatedCoins, generatedCoinCount);
        }else {
            printf("NO CS returned\n");
        }
      } else if (user.actions[i].operation == DEPOSIT_OP) {
          // printf("DEPOSIT %lld as %u STRAT %u\n", transaction_amount, user.type, strategy);
        // Simulate withdrawal
        CoinSelectionResult allocatedCoins = allocate_coins_for_deposit(
            user.wallet, transaction_amount, strategy, user.actions[i].time, denomination_wallet);

        // print_deposit_status(i, transaction_amount, &user.wallet, &allocatedCoins);

        if (!allocatedCoins.coins) {
          char error[1024];
          sprintf(error,
                  "No coins allocated. %d coins for denomination %lld. [%s]",
                  user.wallet.num_coins, transaction_amount,
                  StrategyNames[strategy]);
        } else {

          fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d\n", user_index, i,
                  user.actions[i].time, transaction_amount,
                  OperationNames[user.actions[i].operation], allocatedCoins.tab.deposit_fee_sum,
                  user.wallet.num_coins);

          // printf("Before: %d coins\t\t\n", user.wallet.num_coins);
          remove_selected_coins(&user.wallet, allocatedCoins.coins,
                                allocatedCoins.coin_count);
          // printf("After: %d coins\n", user.wallet.num_coins);

          long long changeAmount = allocatedCoins.tab.effective_amount - transaction_amount;

          // printf("%lld - %lld - %lld - %d", transaction_amount, allocatedAmount, changeAmount, allocatedCoins.coin_count);

            int changeCoinCount = 0;
            Coin *changeCoins =
                generate_withdraw_coins(changeAmount, user.actions[i].time,
                                        denomination_wallet, &changeCoinCount);

            fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d\n", user_index, i,
                    user.actions[i].time, transaction_amount,
                    OperationNames[REFRESH_OP], allocatedCoins.tab.refresh_fee_sum,
                    user.wallet.num_coins);

            fprintf(fp, "%d_%d, %lld, %lld, DEPOSIT_REFRESH_OP, %lld, %d\n",
                    user_index, i, user.actions[i].time, transaction_amount,
                    allocatedCoins.tab.deposit_fee_sum + allocatedCoins.tab.refresh_fee_sum,
                    user.wallet.num_coins + changeCoinCount);
            if (changeCoins) {
              add_coins_to_wallet(&user.wallet, changeCoins, changeCoinCount);
            } else {
              printf("Error for allocation of change coins\n");
            }
          total_fee += allocatedCoins.tab.refresh_fee_sum + allocatedCoins.tab.deposit_fee_sum;
        }
      }
// printf("[%s][%lld] finished.\n", StrategyNames[strategy], user.actions[i].time);
    
    save_wallet_state(user.wallet, user.actions[i].time, log_fp);
    }
  } else {
    printf("No actions generated for the user.\n");
  }
    fclose(fp);

    // remove illegal trailing comma and close outer list
    fseek(log_fp, -2, SEEK_END);
    fprintf(log_fp, "]");
    fclose(log_fp);

  if (user.wallet.coins != NULL) {
      free(user.wallet.coins);
  }
  free(user.actions);
}



