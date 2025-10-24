//
// Created by Bohdan Potuzhnyi on 29.04.2024.
//
#include "simulation.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cjson/cJSON.h"
#include "coin_selection.h"
#include "common.h"
#include "fee.h"
#include "simulation.h"

const char *TypeNames[] = {"STUDENT", "STUDENT_STATIC", "BUSINESS_OWNER",
                         "RETIRED", "FAMILY",         "FREELANCER",
                         "TEACHER", "ARTIST", "BALANCED_ARTIST"};
const char *OperationNames[] = {"DEPOSIT_OP", "WITHDRAW_OP", "REFUND_OP",
                              "REFRESH_OP", "WIRE_OP",     "CLOSE_OP"};

void init(CPUTimer *self) {
    self->accum = 0;
}

float read_timer(CPUTimer *self){ 
    if(self->start_time == (clock_t) - 1) {
        return (float)(clock() - self->start_time + self->accum) / CLOCKS_PER_SEC;
    }
    return (float)self->accum / CLOCKS_PER_SEC;
}

void pause_timer(CPUTimer *self) {
    self->accum += (clock() - self->start_time);
    self->start_time = (clock_t)-1; 
}
void start(CPUTimer *self) {
    self->start_time = clock(); 
}

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

    printf("[%d]\tAmt: %lld, Wallet[%d]: %lld\t\tALLOC[%d]: [%lld\t%lld]\n",i, transaction_amount, wallet->num_coins, wallet_balance, allocatedCoins->coin_count, alloc_balance, allocatedCoins->tab.effective_amount);
}

void close_json_array_file(FILE *file_handle) {
    fseek(file_handle, -2, SEEK_END);
    fprintf(file_handle, "]");
    fclose(file_handle);
}

void add_coins_to_array(Coin *coins, int num_coins, cJSON *json) {

    for(int i = 0; i < num_coins; i++) {
        cJSON *coin_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(coin_json, "value", coins[i].amount);
        cJSON_AddNumberToObject(coin_json, "denomination", coins[i].denomination.amount);
        cJSON_AddItemToArray(json, coin_json);
    }
    
}

void save_wallet_state(Wallet wallet, int i, long long time, FILE *file_handle) {
    cJSON *json = cJSON_CreateObject(); 
    cJSON_AddNumberToObject(json, "Id", i);
    cJSON_AddNumberToObject(json, "Time", time);

    
    cJSON *coins_json = cJSON_AddArrayToObject(json, "coins");
    add_coins_to_array(wallet.coins, wallet.num_coins, coins_json);

    char *json_str = cJSON_PrintUnformatted(json);
    fputs(json_str, file_handle);
    fprintf(file_handle, ",\n");

    cJSON_Delete(json);
    free(json_str);
}

void save_action(int i, long long time, long transaction_amount, int operation, long long fee_for_action, Coin *coins, int num_coins, FILE *file_handle) {
    cJSON *json = cJSON_CreateObject(); 
    cJSON_AddNumberToObject(json, "Id", i);
    cJSON_AddNumberToObject(json, "Time", time);
    cJSON_AddNumberToObject(json, "Amount", transaction_amount);
    cJSON_AddNumberToObject(json, "Operation", operation);
    cJSON_AddNumberToObject(json, "Fee", fee_for_action);
    cJSON_AddNumberToObject(json, "CoinCount", num_coins);

    cJSON *coins_json = cJSON_AddArrayToObject(json, "coins");
    add_coins_to_array(coins, num_coins, coins_json);

    char *json_str = cJSON_PrintUnformatted(json);
    fputs(json_str, file_handle);
    fprintf(file_handle, ",\n");

    cJSON_Delete(json);
    free(json_str);
}

int compare_long(const void *a, const void *b) {
    long diff = *(long*)a - *(long*)b;
    return (diff > 0) - (diff < 0);
}

void save_csv(FILE* fp, User *user, int i, long long fee, operation_type operation, long long *denoms, int num_denoms){
    fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d", (int) user->type, i, user->actions[i].time, user->actions[i].amount, OperationNames[operation], fee, user->wallet.num_coins);

    long long *counts = calloc(num_denoms, sizeof(long long));

    for (int c = 0; c < user->wallet.num_coins ; c++) {
        long long denom_amount = user->wallet.coins[c].denomination.amount;
        // printf("Coin: %lld\n", denom_amount);
        long long *found = bsearch(&denom_amount, denoms, num_denoms, sizeof(long long), compare_long);
        int index = 1 + (int)(found - denoms);

        // printf("indexung %d, %lld\n", index, denom_amount);
        counts[index]++;
    }

    for (int index = 0; index < num_denoms; index++){ 
        fprintf(fp, ", %lld, %lld", counts[index], denoms[index]);
        // printf("%lld\t%lld\n", denoms[denom], counts[denom]);
    }
    fprintf(fp, "\n");

    free(counts);
}


long long *sorted_denominations(Wallet *wallet) {
    long long *denoms = malloc(sizeof(long long) * wallet->num_coins);

    if(denoms == NULL) {
        printf("Malloc failed\n");
        exit(1);
    }
    
    for (int i = 0; i < wallet->num_coins; i++) {
        denoms[i] = wallet->coins[i].denomination.amount;
    }

    qsort(denoms, wallet->num_coins, sizeof(long long), compare_long);

    return denoms;
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

  const int generation_scale = 7;
  long long wallet_scale = 0;

  for (int i = 0; i < denomination_wallet.num_coins; i++) {
    if (wallet_scale < denomination_wallet.coins[i].denomination.amount)
      wallet_scale = denomination_wallet.coins[i].denomination.amount;
  }

  wallet_scale = get_scale(wallet_scale);

  long long *sorted_denom_values = sorted_denominations(&denomination_wallet);

  char coins_log_fname[256];
  sprintf(coins_log_fname, "../simulation/results/coins_user_%d_strategy_%s.json", user_index, StrategyNames[strategy]);
  FILE *coins_log_fp = fopen(coins_log_fname, "w");
  fprintf(coins_log_fp, "[");

  char action_log_fname[256];
  sprintf(action_log_fname, "../simulation/results/actions_user_%d_strategy_%s.json", user_index, StrategyNames[strategy]);
  FILE *action_log_fp = fopen(action_log_fname, "w");
  fprintf(action_log_fp, "[");

  char stat_log_fname[256];
  sprintf(action_log_fname, "../simulation/results/stats_user_%d_strategy_%s.ccsv", user_index, StrategyNames[strategy]);
  FILE *stat_log_fp = fopen(action_log_fname, "w");


  char filename[256];
  sprintf(filename, "../simulation/results/user_%d_strategy_%s.csv", user_index,
          StrategyNames[strategy]);

  FILE *fp = fopen(filename, "w");
  if (!fp) {
    printf("Failed to open file %s for writing.\n", filename);
  }

  CPUTimer timer;
  timer.accum = 0;
  timer.start_time = 0;

  user.wallet.num_coins = 0;

  if (user.actions != NULL && num_actions > 0) {
    fprintf(fp, "%s, %s\n", TypeNames[user.type], StrategyNames[strategy]);
    // printf("Starting sim: %s, %s\n", TypeNames[user.type], StrategyNames[strategy]);

    // save_wallet_state(user.wallet, -1, -1, coins_log_fp);

    for (int i = 0; i < num_actions; i++) {
        // printf("\n");
        // printf("STEP %i: Wallet Balance: %lld\n", i, coins_balance(user.wallet.coins, user.wallet.num_coins));
        // printf("[%s][%lld]\t%s [%lld]\n", StrategyNames[strategy], user.actions[i].time, OperationNames[user.actions[i].operation], user.actions[i].amount);
      
      // STEP Refresh old coin

        long long renew_fee = 0;
        int num_renew_coins = 0;
        Coin *fresh_coins = refresh_old_coins(user.wallet, user.actions[i].time, &num_renew_coins, &renew_fee);

      if (renew_fee > 0) {
         save_csv(fp, &user, i, 0ll, REFRESH_OP, sorted_denom_values, denomination_wallet.num_coins);

        refresh_dirty_coins(&user.wallet, denomination_wallet, user.actions[i].time);
        // save_action(i, user.actions[i].time, 0ll, REFRESH_OP, renew_fee, fresh_coins, num_renew_coins, action_log_fp);
      }

      long long fee_for_action = 0;

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
        long long withdraw_fee = 0;
        Coin *generatedCoins =
            generate_withdraw_coins(transaction_amount, user.actions[i].time,
                                    denomination_wallet, &generatedCoinCount, &withdraw_fee, TRUE);
        if (generatedCoins) {
            save_csv(fp, &user, i, withdraw_fee, user.actions[i].operation, sorted_denom_values, denomination_wallet.num_coins);

        // save_action(i, user.actions[i].time, transaction_amount, user.actions[i].operation, fee_for_action, generatedCoins, generatedCoinCount, action_log_fp);

          add_coins_to_wallet(&user.wallet, generatedCoins, generatedCoinCount);
        // printf("WITHDRAWING +%i\t=%i\n", generatedCoinCount, user.wallet.num_coins);
        }else {
            printf("NO CS returned\n");
        }
      } else if (user.actions[i].operation == DEPOSIT_OP) {
          // printf("DEPOSIT %lld as %u STRAT %u\n", transaction_amount, user.type, strategy);
        // Simulate withdrawal

        start(&timer);
        CoinSelectionResult allocatedCoins = allocate_coins_for_deposit(
            user.wallet, transaction_amount, strategy, user.actions[i].time, denomination_wallet);
        pause_timer(&timer);

        // print_deposit_status(i, transaction_amount, &user.wallet, &allocatedCoins);

        if (!allocatedCoins.coins) {
          char error[1024];
          sprintf(error,
                  "No coins allocated. %d coins for denomination %lld. [%s]",
                  user.wallet.num_coins, transaction_amount,
                  StrategyNames[strategy]);
        } else {
            save_csv(fp, &user, i, allocatedCoins.tab.deposit_fee_sum, DEPOSIT_OP, sorted_denom_values, denomination_wallet.num_coins);
          // fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d\n", user_index, i,
          //         user.actions[i].time, allocatedCoins.tab.effective_amount,
          //         OperationNames[user.actions[i].operation], allocatedCoins.tab.deposit_fee_sum,
          //         user.wallet.num_coins);

          // save_action(i, user.actions[i].time, transaction_amount, user.actions[i].operation, allocatedCoins.tab.deposit_fee_sum + allocatedCoins.tab.refresh_fee_sum, allocatedCoins.coins, allocatedCoins.coin_count, action_log_fp);

          // printf("Before: %d coins\t\t\n", user.wallet.num_coins);
          // remove_selected_coins(&user.wallet, allocatedCoins.coins,
          //                       allocatedCoins.coin_count);
          // printf("After: %d coins\n", user.wallet.num_coins);
          spend_coin_selection(allocatedCoins.coins, allocatedCoins.coin_count, &user.wallet);
          refresh_dirty_coins(&user.wallet, denomination_wallet, user.actions[i].time);
          // printf("Remove after deposit\n");
          // remove_selected_coins(&user.wallet, allocatedCoins.coins, allocatedCoins.coin_count);


          // printf("After: %lld\n", coins_balance(user.wallet.coins, user.wallet.num_coins));
          // printf("DEPOSIT -%i\t=%i\n", allocatedCoins.coin_count, user.wallet.num_coins);
          // long long changeAmount = allocatedCoins.tab.effective_amount - transaction_amount;
          //
          // if (changeAmount > 0){          
          //   int changeCoinCount = 0;
          //   Coin *changeCoins =
          //       generate_withdraw_coins(changeAmount, user.actions[i].time,
          //                               denomination_wallet, &changeCoinCount);

          save_csv(fp, &user, i, allocatedCoins.tab.refresh_fee_sum, REFRESH_OP, sorted_denom_values, denomination_wallet.num_coins);
            // fprintf(fp, "%d_%d, %lld, %lld, %s, %lld, %d\n", user_index, i,
            //         user.actions[i].time, 0l,
            //         OperationNames[REFRESH_OP], allocatedCoins.tab.refresh_fee_sum,
            //         user.wallet.num_coins);
            //
            // fprintf(fp, "%d_%d, %lld, %lld, DEPOSIT_REFRESH_OP, %lld, %d\n",
            //         user_index, i, user.actions[i].time, transaction_amount,
            //         allocatedCoins.tab.deposit_fee_sum + allocatedCoins.tab.refresh_fee_sum,
            //         user.wallet.num_coins + changeCoinCount);
          //   if (changeCoins) {
          //     save_action(i, user.actions[i].time, changeAmount, REFRESH_OP, fee_for_action, allocatedCoins.coins, allocatedCoins.coin_count, action_log_fp);
          //     add_coins_to_wallet(&user.wallet, changeCoins, changeCoinCount);
          //   } else {
          //     printf("Error for allocation of change coins\n");
          //   }  
          // }
        }
      }
// printf("[%s][%lld] finished.\n", StrategyNames[strategy], user.actions[i].time);
    
    // save_wallet_state(user.wallet, i, user.actions[i].time, coins_log_fp);
    }
  } else {
    printf("No actions generated for the user.\n");
  }
   // printf("Timer: %f\n", read_timer(&timer));
    fprintf(stat_log_fp, "%s,%s,%f\n", TypeNames[user_index], StrategyNames[strategy], read_timer(&timer));
    fclose(fp);

   close_json_array_file(coins_log_fp);
   close_json_array_file(action_log_fp);

  if (user.wallet.coins != NULL) {
      // free(user.wallet.coins);
  }
  free(user.actions);
  free(sorted_denom_values);
}



