//coin
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 04.03.2024.
// coin_selection.c
//

#include "common.h"
#include <sched.h>
#include <stddef.h>
#include <unistd.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "coin_selection.h"
#include <stdio.h>
#include <stdlib.h>

#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

const char* StrategyNames[] = {
        "MAX_BILLS",

        "MIN_BILLS",
        "CLOSEST_TO_EXPIRE_MIN_BILLS",
        "CLOSEST_TO_EXPIRE_MAX_BILLS",
        "MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED",
        "RANDOM",
        "EVEN_FROM_MIN_TO_MAX",
        "EVEN_FROM_MAX_TO_MIN",
        "GREEDY_MIN_TO_MAX",
        "GREEDY_MIN_TO_MAX_FIX",
        "GREEDY_MAX_TO_MIN_FIX",
        "CALL_EXTERNAL",
        "WALLET_CORE"
};

/// Global variable to generate unique IDs for coins
static long long nextUniqueId = 1;

static PyObject *pName, *pModule, *pFunc, *pValue, *kwargs, *pFuncRefresh,
    *pValueRefresh, *kwargsRefresh;

int checkOrLoadPython() {

  PyStatus status;

  PyConfig config;

  if (pName != NULL) {
    return 1;
  }

  PyConfig_InitPythonConfig(&config);

  status = Py_InitializeFromConfig(&config);
  if (PyStatus_Exception(status)) {
    PyErr_Print();
    printf("init error\n");
    return 0;
  }
  PyConfig_Clear(&config);

  pName = PyUnicode_FromString("main");
  /* Error checking of pName left out */

  pModule = PyImport_Import(pName);
  Py_DECREF(pName);

  if (pModule != NULL) {
    pFunc = PyObject_GetAttrString(pModule, "process_call");
    if (!pFunc || !PyCallable_Check(pFunc)) {
      printf("Failed to load Python\n");
      return 0;
    }
    pFuncRefresh = PyObject_GetAttrString(pModule, "process_call_refresh");
    if (!pFuncRefresh || !PyCallable_Check(pFuncRefresh)) {
      printf("Failed to load Refresh Function\n");
      return 0;
    }
    return 1;
  }

  PyErr_Print();
  return 0;
}



/**
 * @brief Calculate the score of a coin based on its expiration time and
 * denomination.
 *
 * @param coin The coin whose score is to be calculated.
 * @param currentTime The current time in seconds.
 * @param maxDenom The maximum denomination value.
 * @param minDenom The minimum denomination value.
 * @return The calculated score of the coin.
 */
double calculate_coin_score(Coin *coin, long long currentTime,
                            long long maxDenom, long long minDenom) {

  const double MAX_SCORE = 100.0;
  const double WEIGHT_TIME = 0.8;
  const double WEIGHT_DENOM = 0.2;

  // Time to expiration score calculation
  long long timeLeft = (coin->creation_timestamp +
                        coin->denomination.rules.durations.legal.time) -
                       currentTime;
  long long maxTime = coin->denomination.rules.durations.legal.time;
  double scoreTime = MAX_SCORE * ((double)timeLeft / (double)maxTime);

  // Denomination score calculation
  double normalizedDenom = (double)(coin->denomination.amount - minDenom) /
                           (double)(maxDenom - minDenom);
  double scoreDenom = MAX_SCORE * (1.0 - normalizedDenom);

  // Combine scores with weights
  double finalScore = (WEIGHT_TIME * scoreTime) + (WEIGHT_DENOM * scoreDenom);

  return finalScore;
}

/**
 * mount desc, deposit fee asc, denomPub asc
 */
int compare_wallet_core(const void *a, const void *b) {
  Coin *coinA = (Coin *)a;
  Coin *coinB = (Coin *)b;
  int result = (coinB->denomination.amount - coinA->denomination.amount);

  if (result != 0) {
    return result;
  }

  result = (coinB->denomination.rules.fees.deposit_fee.fee_satoshis -
            coinA->denomination.rules.fees.deposit_fee.fee_satoshis);

  if (result != 0) {
    return result;
  }

  return coinA->uniqueId < coinB->uniqueId;
}

/**
 * @brief Comparison function for sorting coins by creation timestamp in
 * ascending order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin's creation timestamp is less than, equal to, or greater than the second
 * coin's creation timestamp, respectively.
 */
int compare_creation_time_asc(const void *a, const void *b) {
  const Coin *coinA = (const Coin *)a;
  const Coin *coinB = (const Coin *)b;
  return (coinA->creation_timestamp > coinB->creation_timestamp) -
         (coinA->creation_timestamp < coinB->creation_timestamp);
}

/**
 * @brief Comparison function for sorting denominations in ascending order.
 *
 * @param a Pointer to the first denomination.
 * @param b Pointer to the second denomination.
 * @return An integer less than, equal to, or greater than zero if the first
 * denomination is less than, equal to, or greater than the second denomination,
 * respectively.
 */
int compare_denomination_asc(const void *a, const void *b) {
  long long denomA = *(const long long *)a;
  long long denomB = *(const long long *)b;
  return (denomA > denomB) - (denomA < denomB);
}

/**
 * @brief Comparison function for sorting denominations in descending order.
 *
 * @param a Pointer to the first denomination.
 * @param b Pointer to the second denomination.
 * @return An integer less than, equal to, or greater than zero if the first
 * denomination is greater than, equal to, or less than the second denomination,
 * respectively.
 */
int compare_denomination_desc_ll(const void *a, const void *b) {
  long long denomA = *(const long long *)a;
  long long denomB = *(const long long *)b;
  return (denomB > denomA) - (denomB < denomA);
}

/**
 * @brief Comparison function for sorting coin wrappers by score in descending
 * order.
 *
 * @param a Pointer to the first coin wrapper.
 * @param b Pointer to the second coin wrapper.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin wrapper's score is less than, equal to, or greater than the second coin
 * wrapper's score, respectively.
 */
int compare_coin_wrappers(const void *a, const void *b) {
  const coin_wrapper *wrapperA = (const coin_wrapper *)a;
  const coin_wrapper *wrapperB = (const coin_wrapper *)b;
  if (wrapperA->score < wrapperB->score)
    return 1; // Descending order
  if (wrapperA->score > wrapperB->score)
    return -1;
  return 0;
}

/**
 * @brief Comparison function for sorting coins by denomination amount in
 * descending order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin's denomination amount is greater than, equal to, or less than the second
 * coin's denomination amount, respectively.
 */
int compare_coins_desc(const void *a, const void *b) {
  Coin *coinA = (Coin *)a;
  Coin *coinB = (Coin *)b;
  return (coinB->denomination.amount - coinA->denomination.amount);
}

/**
 * @brief Comparison function for sorting coins by denomination amount in
 * ascending order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin's denomination amount is less than, equal to, or greater than the second
 * coin's denomination amount, respectively.
 */
int compare_coins_asc(const void *a, const void *b) {
  Coin *coinA = (Coin *)a;
  Coin *coinB = (Coin *)b;
  return -(coinB->denomination.amount - coinA->denomination.amount);
}

// returns amount plus (deposit & refresh)fees for this coin to be spend
long long amount_using_this(long long amount, Coin *coin) {
  if (amount == coin->denomination.amount) {
    return amount + coin->denomination.rules.fees.deposit_fee.fee_satoshis;
  }
  return amount + coin->denomination.rules.fees.deposit_fee.fee_satoshis +
         coin->denomination.rules.fees.deposit_fee.fee_satoshis;
}

// returns the amount that a merchant would receive spending the whole coin
long long effective_amount(Coin *coin) {
  return coin->denomination.amount -
         coin->denomination.rules.fees.deposit_fee.fee_satoshis;
}

long long total_spending_amount(Wallet *wallet) {
    long long total = 0;

    for (int i = 0; i < wallet->num_coins; i++) {
        total += effective_amount(&wallet->coins[i]);
    }
    return total;
}   

void check_for_dirty_coins(Wallet *wallet) {
    for (int i = 0; i < wallet->num_coins; i++) {
        if(wallet->coins[i].amount != wallet->coins[i].denomination.amount) {
            printf("Dirty Coin found: %lld/%lld", wallet->coins[i].amount, wallet->coins[i].denomination.amount);
        }
    }
}

/**
 * @brief Allocate coins from the wallet to maximize the number of bills used.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin *allocate_max_bills(Wallet wallet, long long amount,
                         int *num_allocated_coins) {
  Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
  if (coinsCopy == NULL)
    return NULL;
  for (int i = 0; i < wallet.num_coins; i++) {
    coinsCopy[i] = wallet.coins[i];
  }

  // Sort coins in descending order based on their amount
  qsort(coinsCopy, wallet.num_coins, sizeof(Coin), compare_coins_desc);

  long long amount_collected = 0;
  int selectedCount = 0;
  for (int j = 0; j < wallet.num_coins && amount_collected < amount; j++) {
    amount_collected += effective_amount(&coinsCopy[j]);
    selectedCount++;
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (selectedCoins == NULL) {
    free(coinsCopy);
    return NULL;
  }

  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &coinsCopy[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
    }
    coin_k->amount = partial_amount;
    selectedCoins[k] = *coin_k;
  }

  *num_allocated_coins = selectedCount;

  free(coinsCopy);
  return selectedCoins;
}

/**
 * @brief Allocate coins from the wallet to minimize the number of bills used.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin *allocate_min_bills(Wallet wallet, long long amount,
                         int *num_allocated_coins) {
  Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
  if (coinsCopy == NULL)
    return NULL;
  for (int i = 0; i < wallet.num_coins; i++) {
    coinsCopy[i] = wallet.coins[i];
  }

  // Sort coins in descending order based on their amount
  qsort(coinsCopy, wallet.num_coins, sizeof(Coin), compare_coins_asc);

  long long amount_collected = 0;
  int selectedCount = 0;
  for (int j = 0; j < wallet.num_coins && amount_collected < amount; j++) {
    amount_collected += coinsCopy[j].denomination.amount;
    selectedCount++;
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (selectedCoins == NULL) {
    free(coinsCopy);
    return NULL; // Allocation failed
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &coinsCopy[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
    }
    coin_k->amount = partial_amount;
    selectedCoins[k] = *coin_k;
  }

  *num_allocated_coins = selectedCount; // Update the number of allocated coins

  free(coinsCopy);
  return selectedCoins;
}

/**
 * @brief Comparison function for sorting coins by expiration time, then by
 * denomination amount.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin's expiration time is less than, equal to, or greater than the second
 * coin's expiration time, respectively. If the expiration times are equal, it
 * compares by denomination amount.
 */
int compare_expiry_time_amount(const void *a, const void *b) {
  const Coin *coinA = (const Coin *)a;
  const Coin *coinB = (const Coin *)b;

  // First, compare by expiration time
  if (coinA->creation_timestamp +
          coinA->denomination.rules.durations.legal.time <
      coinB->creation_timestamp +
          coinB->denomination.rules.durations.legal.time) {
    return -1;
  } else if (coinA->creation_timestamp +
                 coinA->denomination.rules.durations.legal.time >
             coinB->creation_timestamp +
                 coinB->denomination.rules.durations.legal.time) {
    return 1;
  }

  // If expiration times are equal, compare by denomination amount
  return (coinA->denomination.amount > coinB->denomination.amount) -
         (coinA->denomination.amount < coinB->denomination.amount);
}

/**
 * @brief Allocate coins from the wallet that are closest to expiration and
 * minimize the number of bills used.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin *allocate_closest_to_expire_min_bills(Wallet wallet, long long amount,
                                           int *num_allocated_coins) {
  // Allocate memory to copy coins
  Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
  if (coinsCopy == NULL)
    return NULL; // Check if malloc failed

  // Copy coins from the wallet
  for (int i = 0; i < wallet.num_coins; i++) {
    coinsCopy[i] = wallet.coins[i];
  }

  // Sort coins by expiration time, then by amount (both in ascending order)
  qsort(coinsCopy, wallet.num_coins, sizeof(Coin), compare_expiry_time_amount);

  long long amount_collected = 0;
  int selectedCount = 0;
  for (int j = 0; j < wallet.num_coins && amount_collected < amount; j++) {
    if (amount_collected <= amount) {
      amount_collected += coinsCopy[j].denomination.amount;
      selectedCount++;
    } else {
      // If adding this coin exceeds the amount, break the loop
      break;
    }
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (selectedCoins == NULL) {
    free(coinsCopy);
    return NULL; // Allocation failed
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &coinsCopy[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
    }
    coin_k->amount = partial_amount;
    selectedCoins[k] = *coin_k;
  }

  *num_allocated_coins = selectedCount; // Update the number of allocated coins

  free(coinsCopy);
  return selectedCoins; // Return the array of selected coins
}

/**
 * @brief Comparison function for sorting coins by expiration time, then by
 * denomination amount in reverse order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin's expiration time is less than, equal to, or greater than the second
 * coin's expiration time, respectively. If the expiration times are equal, it
 * compares by denomination amount in reverse order.
 */
int compare_expiry_time_amount_reverse(const void *a, const void *b) {
  const Coin *coinA = (const Coin *)a;
  const Coin *coinB = (const Coin *)b;

  // First, compare by expiration time
  if (coinA->creation_timestamp +
          coinA->denomination.rules.durations.legal.time <
      coinB->creation_timestamp +
          coinB->denomination.rules.durations.legal.time) {
    return -1;
  } else if (coinA->creation_timestamp +
                 coinA->denomination.rules.durations.legal.time >
             coinB->creation_timestamp +
                 coinB->denomination.rules.durations.legal.time) {
    return 1;
  }

  // If expiration times are equal, compare by denomination amount in reverse
  // order
  return (coinB->denomination.amount > coinA->denomination.amount) -
         (coinB->denomination.amount < coinA->denomination.amount);
}

/**
 * @brief Allocate coins from the wallet that are closest to expiration and
 * maximize the number of bills used.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin *allocate_closest_to_expire_max_bills(Wallet wallet, long long amount,
                                           int *num_allocated_coins) {
  // Allocate memory to copy coins
  Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
  if (coinsCopy == NULL)
    return NULL; // Check if malloc failed

  // Copy coins from the wallet
  for (int i = 0; i < wallet.num_coins; i++) {
    coinsCopy[i] = wallet.coins[i];
  }

  // Sort coins by expiration time, then by amount in reverse order
  qsort(coinsCopy, wallet.num_coins, sizeof(Coin),
        compare_expiry_time_amount_reverse);

  long long amount_collected = 0;
  int selectedCount = 0;
  for (int j = 0; j < wallet.num_coins && amount_collected < amount; j++) {
    if (amount_collected <= amount) {
      amount_collected += coinsCopy[j].denomination.amount;
      selectedCount++;
    } else {
      // If adding this coin exceeds the amount, break the loop
      break;
    }
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (selectedCoins == NULL) {
    free(coinsCopy);
    return NULL; // Allocation failed
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &coinsCopy[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
    }
    coin_k->amount = partial_amount;
    selectedCoins[k] = *coin_k;
  }

  *num_allocated_coins = selectedCount; // Update the number of allocated coins

  free(coinsCopy);
  return selectedCoins; // Return the array of selected coins
}

/**
 * @brief Allocate coins from the wallet randomly until the desired amount is
 * reached.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin *allocate_random_bills(Wallet wallet, long long amount,
                            int *num_allocated_coins, unsigned int *seed) {

  // Create an array of indices representing the coins
  int *indices = malloc(sizeof(int) * wallet.num_coins);
  if (indices == NULL)
    return NULL; // Check if malloc failed for indices

  for (int i = 0; i < wallet.num_coins; i++) {
    indices[i] = i; // Initialize indices with the coin positions
  }

  long long amount_collected = 0;
  Coin *selectedCoins = malloc(
      sizeof(Coin) *
      wallet.num_coins); // Allocate memory to store potentially all coins
  if (selectedCoins == NULL) {
    free(indices);
    return NULL; // Allocation failed
  }

  int selectedCount = 0;
  int remainingCoins = wallet.num_coins;

  while (amount_collected < amount && remainingCoins > 0) {
    int randIndex =
        rand_r(seed) %
        remainingCoins; // Pick a random index from the remaining indices
    int selectedCoinIndex = indices[randIndex];

    // Add the selected coin if it doesn't exceed the desired amount
    if (amount_collected <= amount) {
      amount_collected += wallet.coins[selectedCoinIndex].denomination.amount;
      selectedCoins[selectedCount++] = wallet.coins[selectedCoinIndex];
    }

    // Remove the selected index by replacing it with the last available index
    indices[randIndex] = indices[remainingCoins - 1];
    remainingCoins--;
  }

  // Allocate memory for selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    free(selectedCoins);
    free(indices);
    return NULL; // Allocation failed
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }
  *num_allocated_coins = selectedCount;

  free(selectedCoins);
  free(indices);
  return finalSelectedCoins;
}

/**
 * @brief Allocate coins from the wallet to maximize the number of bills used,
 * weighted by time to expiration.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param currentTime The current time in seconds.
 * @return An array of allocated coins.
 */
Coin *allocate_max_bills_time_to_expire_weighted(Wallet wallet,
                                                 long long amount,
                                                 int *num_allocated_coins,
                                                 long long currentTime) {
  coin_wrapper *wrappers =
      (coin_wrapper *)malloc(sizeof(coin_wrapper) * wallet.num_coins);
  if (!wrappers)
    return NULL;

  long long maxDenom = -100;
  long long minDenom = 1000000000;

  for (int i = 0; i < wallet.num_coins; ++i) {
    if (maxDenom > wallet.coins[i].denomination.amount) {
      maxDenom = wallet.coins[i].denomination.amount;
    }
    if (minDenom < wallet.coins[i].denomination.amount) {
      minDenom = wallet.coins[i].denomination.amount;
    }
  }

  // Populate wrappers and compute scores
  for (int i = 0; i < wallet.num_coins; ++i) {
    wrappers[i].coin = &wallet.coins[i];
    wrappers[i].score =
        calculate_coin_score(wrappers[i].coin, currentTime, maxDenom, minDenom);
  }

  // Sort the wrappers by score in descending order
  qsort(wrappers, wallet.num_coins, sizeof(coin_wrapper),
        compare_coin_wrappers);

  // Allocate and select coins based on sorted wrappers until the desired amount
  // is reached

  *num_allocated_coins = 0;
  long long allocated_amount = 0;
  for (int i = 0; i < wallet.num_coins && allocated_amount < amount; ++i) {
    allocated_amount += wrappers[i].coin->denomination.amount;
    (*num_allocated_coins)++;
  }

  // Allocate memory for selected coins and copy them
  Coin *selectedCoins = malloc(sizeof(Coin) * (*num_allocated_coins));
  if (!selectedCoins) {
    free(wrappers);
    return NULL;
  }

  // Copy selected coins
  long long amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < *num_allocated_coins; k++) {
    Coin *coin_k = wrappers[k].coin;
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
    }
    coin_k->amount = partial_amount;
    selectedCoins[k] = *coin_k;
  }

  free(wrappers);
  return selectedCoins;
}

/**
 * @brief Allocate coins from the wallet evenly from the smallest to the largest
 * denomination.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination
 * information.
 * @return An array of allocated coins.
 */
Coin *allocate_coins_even_from_min_to_max(Wallet wallet, long long amount,
                                          int *num_allocated_coins,
                                          Wallet denomination_wallet) {
  // Sort coins in the wallet by their creation timestamp in ascending order
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin),
        compare_creation_time_asc);

  // Allocate memory for the 2D array
  int num_denominations = denomination_wallet.num_coins;
  long long **denom_array = malloc(2 * sizeof(long long *));
  if (denom_array == NULL)
    return NULL; // Check if malloc failed

  denom_array[0] =
      malloc(num_denominations * sizeof(long long)); // For denominations
  denom_array[1] =
      malloc(num_denominations * sizeof(long long)); // For quantities
  if (denom_array[0] == NULL || denom_array[1] == NULL) {
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL; // Check if malloc failed
  }

  // Initialize the denominations from the denomination wallet
  for (int i = 0; i < num_denominations; i++) {
    denom_array[0][i] = denomination_wallet.coins[i].denomination.amount;
    denom_array[1][i] = 0; // Initialize quantity to zero
  }

  // Sort the denominations in ascending order
  qsort(denom_array[0], num_denominations, sizeof(long long),
        compare_denomination_asc);

  // Update the quantity array with the actual quantities of the denominations
  // from the wallet
  for (int i = 0; i < wallet.num_coins; i++) {
    for (int j = 0; j < num_denominations; j++) {
      if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
        denom_array[1][j]++;
        break;
      }
    }
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
  if (selectedCoins == NULL) {
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL; // Allocation failed
  }

  long long amount_collected = 0;
  int selectedCount = 0;
  int *selectedFlags =
      malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
  if (selectedFlags == NULL) {
    free(selectedCoins);
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL;
  }
  for (int i = 0; i < wallet.num_coins; i++) {
    selectedFlags[i] = 0; // Mark all coins as not selected
  }

  // Select coins evenly from the smallest to the largest denomination
  while (amount_collected < amount) {
    int anyAdded = 0;
    for (int i = 0; i < num_denominations; i++) {
      if (denom_array[1][i] > 0) {
        for (int j = 0; j < wallet.num_coins; j++) {
          if (wallet.coins[j].denomination.amount == denom_array[0][i] &&
              selectedFlags[j] == 0) {
            selectedCoins[selectedCount++] = wallet.coins[j];
            amount_collected += denom_array[0][i];
            denom_array[1][i]--;
            selectedFlags[j] = 1;
            anyAdded = 1;
            break;
          }
        }
        if (amount_collected >= amount) {
          break;
        }
      }
    }
    if (!anyAdded) {
      break;
    }
  }

  *num_allocated_coins = selectedCount;

  // Resize the selectedCoins array to the actual number of selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    // If realloc failed, free original block and return NULL
    free(selectedCoins);
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    free(selectedFlags);
    return NULL;
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }
  free(selectedCoins);
  free(denom_array[0]);
  free(denom_array[1]);
  free(denom_array);
  free(selectedFlags);

  return finalSelectedCoins;
}

/**
 * @brief Allocate coins from the wallet evenly from the largest to the smallest
 * denomination.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination
 * information.
 * @return An array of allocated coins.
 */
Coin *allocate_coins_even_from_max_to_min(Wallet wallet, long long amount,
                                          int *num_allocated_coins,
                                          Wallet denomination_wallet) {
  // Sort coins in the wallet by their creation timestamp in ascending order
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin),
        compare_creation_time_asc);

  // Allocate memory for the 2D array
  int num_denominations = denomination_wallet.num_coins;
  long long **denom_array = malloc(2 * sizeof(long long *));
  if (denom_array == NULL)
    return NULL; // Check if malloc failed

  denom_array[0] =
      malloc(num_denominations * sizeof(long long)); // For denominations
  denom_array[1] =
      malloc(num_denominations * sizeof(long long)); // For quantities
  if (denom_array[0] == NULL || denom_array[1] == NULL) {
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL; // Check if malloc failed
  }

  // Initialize the denominations from the denomination wallet
  for (int i = 0; i < num_denominations; i++) {
    denom_array[0][i] = denomination_wallet.coins[i].denomination.amount;
    denom_array[1][i] = 0; // Initialize quantity to zero
  }

  // Sort the denominations in ascending order
  qsort(denom_array[0], num_denominations, sizeof(long long),
        compare_denomination_desc_ll);

  // Update the quantity array with the actual quantities of the denominations
  // from the wallet
  for (int i = 0; i < wallet.num_coins; i++) {
    for (int j = 0; j < num_denominations; j++) {
      if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
        denom_array[1][j]++;
        break;
      }
    }
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
  if (selectedCoins == NULL) {
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL; // Allocation failed
  }

  long long amount_collected = 0;
  int selectedCount = 0;
  int *selectedFlags =
      malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
  if (selectedFlags == NULL) {
    free(selectedCoins);
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL;
  }
  for (int i = 0; i < wallet.num_coins; i++) {
    selectedFlags[i] = 0; // Mark all coins as not selected
  }

  // Select coins evenly from the smallest to the largest denomination
  while (amount_collected < amount) {
    int anyAdded = 0;
    for (int i = 0; i < num_denominations; i++) {
      if (denom_array[1][i] > 0) {
        for (int j = 0; j < wallet.num_coins; j++) {
          if (wallet.coins[j].denomination.amount == denom_array[0][i] &&
              selectedFlags[j] == 0) {
            selectedCoins[selectedCount++] = wallet.coins[j];
            amount_collected += denom_array[0][i];
            denom_array[1][i]--;
            selectedFlags[j] = 1;
            anyAdded = 1;
            break;
          }
        }
        if (amount_collected >= amount) {
          break;
        }
      }
    }
    if (!anyAdded) {
      break; // If no coins were added in this pass, stop the loop
    }
  }

  *num_allocated_coins = selectedCount;

  // Resize the selectedCoins array to the actual number of selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    free(selectedCoins);
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    free(selectedFlags);
    return NULL;
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }

  // Clean up
  free(selectedCoins);
  free(denom_array[0]);
  free(denom_array[1]);
  free(denom_array);
  free(selectedFlags);

  return finalSelectedCoins;
}


/**
 * @brief Allocate coins from the wallet using a greedy algorithm from the
 * smallest to the largest denomination.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination
 * information.
 * @return An array of allocated coins.
 */
Coin *allocate_coins_greedy_min_to_max(Wallet wallet, long long amount,
                                       int *num_allocated_coins,
                                       Wallet denomination_wallet) {
  // Sort coins in the wallet by their creation timestamp in ascending order
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin),
        compare_creation_time_asc);

  qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_coins_desc);


  // Allocate memory for the 2D array
  int num_denominations = denomination_wallet.num_coins;
  long long **denom_array = malloc(2 * sizeof(long long *));
  if (denom_array == NULL)
    return NULL; // Check if malloc failed

  denom_array[0] =
      malloc(num_denominations * sizeof(long long)); // For denominations
  denom_array[1] =
      malloc(num_denominations * sizeof(long long)); // For quantities
  if (denom_array[0] == NULL || denom_array[1] == NULL) {
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL; // Check if malloc failed
  }

  // Initialize the denominations from the denomination wallet
  for (int i = 0; i < num_denominations; i++) {
    denom_array[0][i] = denomination_wallet.coins[i].denomination.amount;
    denom_array[1][i] = 0; // Initialize quantity to zero
  }

  // Sort the denominations in ascending order
  qsort(denom_array[0], num_denominations, sizeof(long long),
        compare_denomination_desc_ll);

  // Update the quantity array with the actual quantities of the denominations
  // from the wallet
  for (int i = 0; i < wallet.num_coins; i++) {
    for (int j = 0; j < num_denominations; j++) {
      if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
        denom_array[1][j]++;
        break;
      }
    }
  }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
  if (selectedCoins == NULL) {
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL; // Allocation failed
  }

  long long amount_collected = 0;
  int selectedCount = 0;
  int *selectedFlags =
      malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
  if (selectedFlags == NULL) {
    free(selectedCoins);
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    return NULL;
  }
  for (int i = 0; i < wallet.num_coins; i++) {
    selectedFlags[i] = 0; // Mark all coins as not selected
  }

  int first = 1;

  // Greedy selection algorithm
  while (amount_collected < amount) {
    long long closestAmount = 0;
    int closestIndex = -1;

    // Find the coin that brings us closest to the target amount without
    // exceeding it
    for (int i = 0; i < wallet.num_coins; i++) {
      if (selectedFlags[i] == 0) {
        if (first) {
          closestAmount = effective_amount(&wallet.coins[i]);
          closestIndex = i;
          first = 0;
        }
        long long tempAmount = amount_collected + effective_amount(&wallet.coins[i]);
        if (tempAmount <= amount && tempAmount > closestAmount) {
          closestAmount = tempAmount;
          closestIndex = i;
        }
      }
    }

    // If no coin can be added without exceeding the target, break the loop
    if (closestIndex == -1) {
      break;
    }

    // Select the coin and update the amount collected
    selectedCoins[selectedCount++] = wallet.coins[closestIndex];
    amount_collected = closestAmount;
    selectedFlags[closestIndex] = 1;
  }

  *num_allocated_coins = selectedCount;

  // Resize the selectedCoins array to the actual number of selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    // If realloc failed, free original block and return NULL
    free(selectedCoins);
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    free(selectedFlags);
    return NULL;
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
        partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
        if (selectedCount != 0 && k != selectedCount - 1) {
            printf("ERROR: Assumption violated in allocate_greedy_min_to_max\n");
            exit(1);
        }
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }

  // Clean up
  free(selectedCoins);
  free(denom_array[0]);
  free(denom_array[1]);
  free(denom_array);
  free(selectedFlags);

  return finalSelectedCoins;
}

Coin *allocate_coins_greedy_min_to_max_fix(Wallet wallet, long long amount,
                                           int *num_allocated_coins,
                                           Wallet denomination_wallet) {
  // Sort coins in the wallet by their creation timestamp in ascending order
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_creation_time_asc);
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_coins_desc);

  // // Allocate memory for the 2D array
  // int num_denominations = denomination_wallet.num_coins;
  // long long **denom_array = malloc(2 * sizeof(long long *));
  // if (denom_array == NULL)
  //   return NULL; // Check if malloc failed
  //
  // denom_array[0] =
  //     malloc(num_denominations * sizeof(long long)); // For denominations
  // denom_array[1] =
  //     malloc(num_denominations * sizeof(long long)); // For quantities
  // if (denom_array[0] == NULL || denom_array[1] == NULL) {
  //   free(denom_array[0]);
  //   free(denom_array[1]);
  //   free(denom_array);
  //   return NULL; // Check if malloc failed
  // }
  //
  // // Initialize the denominations from the denomination wallet
  // for (int i = 0; i < num_denominations; i++) {
  //   denom_array[0][i] = denomination_wallet.coins[i].denomination.amount;
  //   denom_array[1][i] = 0; // Initialize quantity to zero
  // }
  //
  // // Sort the denominations in ascending order
  // qsort(denom_array[0], num_denominations, sizeof(long long),
  //       compare_denomination_desc_ll);
  //
  // // Update the quantity array with the actual quantities of the denominations
  // // from the wallet
  // for (int i = 0; i < wallet.num_coins; i++) {
  //   for (int j = 0; j < num_denominations; j++) {
  //     if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
  //       denom_array[1][j]++;
  //       break;
  //     }
  //   }
  // }

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
  if (selectedCoins == NULL) {
    // free(denom_array[0]);
    // free(denom_array[1]);
    // free(denom_array);
    return NULL; // Allocation failed
  }

  long long amount_collected = 0;
  int selectedCount = 0;
  int *selectedFlags =
      malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
  if (selectedFlags == NULL) {
    free(selectedCoins);
    // free(denom_array[0]);
    // free(denom_array[1]);
    // free(denom_array);
    return NULL;
  }
  for (int i = 0; i < wallet.num_coins; i++) {
    selectedFlags[i] = 0; // Mark all coins as not selected
  }

  int first = 1;

  // Greedy selection algorithm
  while (amount_collected < amount) {
    long long closestAmount = 0;
    int closestIndex = -1;

    // Find the coin that brings us closest to the target amount without
    // exceeding it
    for (int i = 0; i < wallet.num_coins; i++) {
      if (selectedFlags[i] == 0) {
        if (first) {
          closestAmount = effective_amount(&wallet.coins[i]);
          closestIndex = i;
          first = 0;
        }
        long long tempAmount = amount_collected + effective_amount(&wallet.coins[i]);
        if (tempAmount > closestAmount) {
          closestAmount = tempAmount;
          closestIndex = i;
        }
      }
    }

    // If no coin can be added without exceeding the target, break the loop
    if (closestIndex == -1) {
      break;
    }

    // Select the coin and update the amount collected
    selectedCoins[selectedCount++] = wallet.coins[closestIndex];
    amount_collected = closestAmount;
    selectedFlags[closestIndex] = 1;
  }

  *num_allocated_coins = selectedCount;

  // Resize the selectedCoins array to the actual number of selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    // If realloc failed, free original block and return NULL
    free(selectedCoins);
    // free(denom_array[0]);
    // free(denom_array[1]);
    // free(denom_array);
    free(selectedFlags);
    return NULL;
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }

  // Clean up
  free(selectedCoins);
  // free(denom_array[0]);
  // free(denom_array[1]);
  // free(denom_array);
  free(selectedFlags);

  return finalSelectedCoins;
}

Coin *allocate_coins_greedy_max_to_min_fix(Wallet wallet, long long amount,
                                           int *num_allocated_coins,
                                           Wallet denomination_wallet) {
  // Sort coins in the wallet by their creation timestamp in ascending order
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_creation_time_asc);
  qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_coins_asc);

  // Allocate memory for selected coins
  Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
  if (selectedCoins == NULL) {
    return NULL; // Allocation failed
  }

  long long amount_collected = 0;
  int selectedCount = 0;
  int *selectedFlags =
      malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
  if (selectedFlags == NULL) {
    free(selectedCoins);
    return NULL;
  }
  for (int i = 0; i < wallet.num_coins; i++) {
    selectedFlags[i] = 0; // Mark all coins as not selected
  }

  for (int i = 0; i < wallet.num_coins; i++) {
       
      amount_collected += effective_amount(&wallet.coins[i]);

    // Select the coin and update the amount collected
    
      selectedCoins[selectedCount++] = wallet.coins[i];  
      selectedFlags[i] = 1;

      if(amount_collected >= amount){
            break;
      }
  }

  *num_allocated_coins = selectedCount;

  // Resize the selectedCoins array to the actual number of selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    // If realloc failed, free original block and return NULL
    free(selectedCoins);
    free(selectedFlags);
    return NULL;
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }

  // Clean up
  free(selectedCoins);
  // free(denom_array[0]);
  // free(denom_array[1]);
  // free(denom_array);
  free(selectedFlags);

  return finalSelectedCoins;
}

PyObject *encodeFee(Fee fee) {
  return Py_BuildValue("(L,f)", fee.fee_satoshis, fee.percentage_fee);
}

PyObject *encodeFees(Fees fees) {
  return Py_BuildValue("(O,O,O,O)", encodeFee(fees.deposit_fee),
                       encodeFee(fees.refund_fee), encodeFee(fees.withdraw_fee),
                       encodeFee(fees.refresh_fee));
}
PyObject *encodeDurations(Durations durations) {
  return Py_BuildValue("(L,L,L)", durations.legal.time, durations.deposit.time,
                       durations.withdraw.time);
}

PyObject *encodeRules(Rules rules) {
  return Py_BuildValue("(i,s,O,O)", rules.rsa_keysize, rules.cipher,
                       encodeFees(rules.fees),
                       encodeDurations(rules.durations));
}

PyObject *encodeDenomination(Denomination denom) {
  return Py_BuildValue("(s,L,O)", denom.name, denom.amount,
                       encodeRules(denom.rules));
}

PyObject *encodeCoin(Coin coin) {
  return Py_BuildValue("(L,O,L,L)", coin.uniqueId,
                       encodeDenomination(coin.denomination),
                       coin.creation_timestamp, coin.amount);
}

PyObject *encodeGlobalFees(GlobalFees global_fees) {
  return Py_BuildValue("(O,O)", encodeFee(global_fees.wire_fee),
                       encodeFee(global_fees.closing_fee));
}

PyObject *encodeWallet(Wallet wallet) {
  // Coin* coins
  // int num_coins
  // GlobalFees globalFees
  PyObject *walletObj, *coins, *globalFees;

  walletObj = PyDict_New();
  coins = PyList_New(0);

  for (int i = 0; i < wallet.num_coins; ++i) {
    PyObject *coin = encodeCoin(wallet.coins[i]);
    PyList_Append(coins, coin);
  }
  PyDict_SetItemString(walletObj, "coins", coins);
  PyDict_SetItemString(walletObj, "global_fees",
                       encodeGlobalFees(wallet.global_fees));
  return walletObj;
}

Coin *allocate_call_external(Wallet wallet, long long amount,
                             int *num_allocated_coins) {

  PyObject *pValue, *kwargs, *pAmount, *indexAmountTuple;
  if (checkOrLoadPython()) {

    kwargs = PyDict_New();

    pAmount = PyLong_FromLong(amount);

    // printf("coins: %i\n", wallet.num_coins);

    PyDict_SetItemString(kwargs, "amount", pAmount);
    PyDict_SetItemString(kwargs, "wallet", encodeWallet(wallet));
    pValue = PyObject_CallOneArg(pFunc, kwargs);

    PyErr_Print();
    // Py_DECREF(kwargs);
    // Py_DECREF(pAmount);

    // Allocate memory for selected coins
    Coin *selectedCoins = malloc(sizeof(Coin) * PyList_Size(pValue));
    if (selectedCoins == NULL) {
      return NULL; // Allocation failed
    }

    // Copy selected coins
    for (int k = 0; k < PyList_Size(pValue); k++) {
        indexAmountTuple = PyList_GetItem(pValue, k);
        long long coinAmount = PyLong_AsLongLong(PyTuple_GetItem(indexAmountTuple, 1));
        selectedCoins[k] = wallet.coins[PyLong_AsInt(PyTuple_GetItem(indexAmountTuple, 0))];
        selectedCoins[k].amount = coinAmount;
    }

    *num_allocated_coins = PyList_Size(pValue);
    
    // Py_DECREF(pValue);
    return selectedCoins;

    // Coin *finalSelectedCoins = malloc(sizeof(Coin) * *num_allocated_coins);
    //
    // if (finalSelectedCoins == NULL) {
    //   free(selectedCoins);
    //   return NULL;
    // }
    //
    // // Copy selected coins
    // long long amount_collected = 0;
    // long long effective;
    // long long partial_amount;
    // for (int k = 0; k < *num_allocated_coins; k++) {
    //   Coin *coin_k = &selectedCoins[k];
    //   effective = effective_amount(coin_k);
    //   if (amount_collected + effective > amount) {
    //     partial_amount =
    //         amount - amount_collected +
    //         coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
    //         coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    //   } else {
    //     partial_amount = coin_k->denomination.amount;
    //   }
    //   coin_k->amount = partial_amount;
    //   finalSelectedCoins[k] = *coin_k;
    // }
    //
    // if (pValue != NULL) {
    //   Py_DECREF(pValue);
    // } else {
    //   printf("NULL result\n");
    // }
    // return finalSelectedCoins;
  } else {
    printf("Python didnt load :/\n");
    return NULL;
  }
  return NULL;
}

Coin *allocate_wallet_core(Wallet wallet, long long amount,
                           int *num_allocated_coins) {
  Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
  if (coinsCopy == NULL)
    return NULL;

  for (int i = 0; i < wallet.num_coins; i++) {
    coinsCopy[i] = wallet.coins[i];
  }

  // Sort coins in descending order based on their amount
  qsort(coinsCopy, wallet.num_coins, sizeof(Coin), compare_wallet_core);

  long long payRemaining = amount;
  Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
  if (selectedCoins == NULL) {
    free(coinsCopy);
    return NULL; // Allocation failed
  }

  // printf("START\n min: %ld, max: %ld", coinsCopy[0].denomination.amount,
  // coinsCopy[wallet.num_coins - 1].denomination.amount);

  int i = 0;

  for (; i < wallet.num_coins && payRemaining > 0; i++) {

    Coin coin = coinsCopy[i];

    if (coin.denomination.amount <
        coin.denomination.rules.fees.deposit_fee.fee_satoshis) {
      continue;
    }

    // TODO: allowance
    payRemaining += coin.denomination.rules.fees.deposit_fee.fee_satoshis;

    // printf("%i\tPayremaining %i\t\nCoinvalue %i\t\n", i, payRemaining,
    // coin.denomination.rules.fees.deposit_fee.fee_satoshis);
    //  TODO: wenn fees beachtet werden
    // long long coinSpend = max(min(payRemaining, coin.denomination.amount),
    // coin.denomination.rules.fees.deposit_fee.fee_satoshis);
    long long coinSpend = coin.denomination.amount;

    payRemaining -= coinSpend;

    selectedCoins[i] = coin;

    // printf("%i\tPayremaining %i\tCoinSPend %i\n\n", i, payRemaining,
    // coinSpend);
  }

  if (payRemaining > 0) {
    free(selectedCoins);
    free(coinsCopy);
    return NULL;
  }

  *num_allocated_coins = i; // Update the number of allocated coins

  // Resize the selectedCoins array to the actual number of selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * i);
  if (finalSelectedCoins == NULL) {
    // If realloc failed, free original block and return NULL
    free(selectedCoins);
    free(coinsCopy);
    return NULL;
  }

  // Copy selected coins
  long long amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < i; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }

  free(coinsCopy);
  free(selectedCoins);
  return finalSelectedCoins;
}

/**
 * @brief Allocate coins from the wallet randomly until the desired amount is
 * reached.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin *allocate_random_improve(Wallet wallet, long long amount,
                            int *num_allocated_coins, unsigned int *seed) {

  // Create an array of indices representing the coins
  int *indices = malloc(sizeof(int) * wallet.num_coins);
  if (indices == NULL)
    return NULL; // Check if malloc failed for indices

  for (int i = 0; i < wallet.num_coins; i++) {
    indices[i] = i; // Initialize indices with the coin positions
  }

  long long amount_collected = 0;
  Coin *selectedCoins = malloc(
      sizeof(Coin) *
      wallet.num_coins); // Allocate memory to store potentially all coins
  if (selectedCoins == NULL) {
    free(indices);
    return NULL; // Allocation failed
  }

  int selectedCount = 0;
  int remainingCoins = wallet.num_coins;

  while (amount_collected < amount && remainingCoins > 0) {
    int randIndex =
        rand_r(seed) %
        remainingCoins; // Pick a random index from the remaining indices
    int selectedCoinIndex = indices[randIndex];

    // Add the selected coin if it doesn't exceed the desired amount
    if (amount_collected <= amount) {
      amount_collected += wallet.coins[selectedCoinIndex].denomination.amount;
      selectedCoins[selectedCount++] = wallet.coins[selectedCoinIndex];
    }

    // Remove the selected index by replacing it with the last available index
    indices[randIndex] = indices[remainingCoins - 1];
    remainingCoins--;
  }

  // Allocate memory for selected coins
  Coin *finalSelectedCoins = malloc(sizeof(Coin) * selectedCount);
  if (finalSelectedCoins == NULL) {
    free(selectedCoins);
    free(indices);
    return NULL; // Allocation failed
  }

  // Copy selected coins
  amount_collected = 0;
  long long effective;
  long long partial_amount;
  for (int k = 0; k < selectedCount; k++) {
    Coin *coin_k = &selectedCoins[k];
    effective = effective_amount(coin_k);
    if (amount_collected + effective > amount) {
      partial_amount =
          amount - amount_collected +
          coin_k->denomination.rules.fees.deposit_fee.fee_satoshis +
          coin_k->denomination.rules.fees.refresh_fee.fee_satoshis;
    } else {
      partial_amount = coin_k->denomination.amount;
      amount_collected += effective;
    }
    coin_k->amount = partial_amount;
    finalSelectedCoins[k] = *coin_k;
  }
  *num_allocated_coins = selectedCount;

  free(selectedCoins);
  free(indices);
  return finalSelectedCoins;
}

void verify_partial_coin(Coin *coin) {
  // assert(coin->amount < coin->denomination.amount);
  // if (coin_part->amount < coin_part->coin->denomination.amount) {
  //   printf("Invalid Coin:\t%lld / %lld\n", coin_part->amount,
  //   coin_part->coin->denomination.amount); exit(0);
  // }
}

long long effective_refresh_fee(Coin *coin) {
  if (coin->amount == coin->denomination.amount || coin->amount == 0) {
    return 0;
  }
  return coin->denomination.rules.fees.refresh_fee.fee_satoshis;
}

FeeTab fees_for_selection(long long amount, Coin *selection, int *num_coins) {
  // TODO: allowance
  long long effective_amount_sum = 0;
  long long deposit_fee_sum = 0;
  long long refresh_fee_sum = 0;

  for (int i = 0; i < *num_coins; i++) {
    Coin *coin_k = &selection[i];

    verify_partial_coin(coin_k);

    effective_amount_sum += coin_k->amount;
    deposit_fee_sum += coin_k->denomination.rules.fees.deposit_fee.fee_satoshis;
    refresh_fee_sum += effective_refresh_fee(coin_k);

    // printf("%lld\t%lld\t%lld\t%lld\n", coin_k->amount, coin_k->denomination.amount, deposit_fee_sum, refresh_fee_sum);
  }

  int allocation_sufficient = effective_amount_sum >= amount;

  FeeTab tab = {.instructed_amount = amount,
                .effective_amount = effective_amount_sum,
                .refresh_fee_sum = refresh_fee_sum,
                .deposit_fee_sum = deposit_fee_sum,
                .valid = allocation_sufficient};

  return tab;
}

int verify_coin_selection(Wallet wallet, long long amount, Coin *selection,
                          int *num_coins) {
  FeeTab tab = fees_for_selection(amount, selection, num_coins);

  // if (!tab.valid) {
  //   printf("Coin-selection invalid: %lld / %lld\n", tab.instructed_amount,
  //          tab.effective_amount);
  //   return 0;
  // }

  return 1;
}

/**
 * @brief Allocate coins from the wallet according to the specified strategy.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param strategy The strategy to use for allocation.
 * @param time The current time in seconds.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination
 * information.
 * @return An array of allocated coins.
 */
CoinSelectionResult allocate_coins_for_deposit(Wallet wallet, long long amount,
                                               strategy strategy,
                                               long long time,
                                               Wallet denomination_wallet) {

  if (!wallet.num_coins || !amount) { // if wallet is empty or amount 0, return
    return (CoinSelectionResult){
        .coins = NULL, .coin_count = 0, .tab = (FeeTab){0}};
  }

  Coin *allocated_coins;
  int num_allocated_coins = 0;

  unsigned int seed = 0;

  // check_for_dirty_coins(&wallet);

  switch (strategy) {
  case MAX_BILLS:
    allocated_coins = allocate_max_bills(wallet, amount, &num_allocated_coins);
    break;
  case MIN_BILLS:
    allocated_coins = allocate_min_bills(wallet, amount, &num_allocated_coins);
    break;
  case CLOSEST_TO_EXPIRE_MIN_BILLS:
    allocated_coins = allocate_closest_to_expire_min_bills(
        wallet, amount, &num_allocated_coins);
    break;
  case CLOSEST_TO_EXPIRE_MAX_BILLS:
    allocated_coins = allocate_closest_to_expire_max_bills(
        wallet, amount, &num_allocated_coins);
    break;
  case MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED:
    allocated_coins = allocate_max_bills_time_to_expire_weighted(
        wallet, amount, &num_allocated_coins, time);
    break;
  case RANDOM:
    allocated_coins =
        allocate_random_bills(wallet, amount, &num_allocated_coins, &seed);
    break;
  case EVEN_FROM_MIN_TO_MAX:
    allocated_coins = allocate_coins_even_from_min_to_max(
        wallet, amount, &num_allocated_coins, denomination_wallet);
    break;
  case EVEN_FROM_MAX_TO_MIN:
    allocated_coins = allocate_coins_even_from_max_to_min(
        wallet, amount, &num_allocated_coins, denomination_wallet);
    break;
  case GREEDY_MIN_TO_MAX:
    allocated_coins = allocate_coins_greedy_min_to_max(
        wallet, amount, &num_allocated_coins, denomination_wallet);
    break;
  case GREEDY_MIN_TO_MAX_FIX:
    allocated_coins = allocate_coins_greedy_min_to_max_fix(
        wallet, amount, &num_allocated_coins, denomination_wallet);
    break;
  case GREEDY_MAX_TO_MIN_FIX:
    allocated_coins = allocate_coins_greedy_max_to_min_fix(
        wallet, amount, &num_allocated_coins, denomination_wallet);
    break;
  case CUSTOM_EXTERNAL:
    allocated_coins =
        allocate_call_external(wallet, amount, &num_allocated_coins);
    break;
  case WALLET_CORE:
    allocated_coins =
        allocate_wallet_core(wallet, amount, &num_allocated_coins);
    break;
  default:
    allocated_coins =
        allocate_random_bills(wallet, amount, &num_allocated_coins, &seed);
  }

  FeeTab tab =
      fees_for_selection(amount, allocated_coins, &num_allocated_coins);

     // printf("TAB: target: %lld, instructed %lld, payed: %lld, D: %lld, R: %lld\n", amount, tab.instructed_amount, tab.effective_amount, tab.deposit_fee_sum, tab.refresh_fee_sum);
     // for (int i = 0; i<num_allocated_coins; i++) {
     //     printf("\tpaying: %lld/%lld\n", allocated_coins[i].amount, allocated_coins[i].denomination.amount);
     // }

  if (!tab.valid) {
    long long total = 0;
    for (int i = 0; i < wallet.num_coins; i++) {
      total += wallet.coins[i].amount;
    }
  
    long long able_to_spend = total_spending_amount(&wallet);
    if (able_to_spend >= amount) {
        // This may happen when the generated Steps attempt to overspend
        printf("Invalid Selection: %d Coins pay for Amount: %lld/%lld\t Wallet has %lld\n", num_allocated_coins, tab.effective_amount, amount, able_to_spend);
        pprint(wallet.coins, wallet.num_coins);
        pprint(allocated_coins, num_allocated_coins);
        exit(1);
    }

    return (CoinSelectionResult){
        .coins = NULL, .coin_count = 0, .tab = (FeeTab){0}};
  }
    //  printf("Wallet:\n");
    //  pprint(wallet.coins, wallet.num_coins);
    //  printf("Spending %u:\n", strategy);
    // pprint(allocated_coins, num_allocated_coins);

  return (CoinSelectionResult){
      .coins = allocated_coins, .coin_count = num_allocated_coins, .tab = tab};
}

/**
 * @brief Comparison function for sorting coins in descending order by
 * denomination amount.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first
 * coin's denomination amount is greater than, equal to, or less than the second
 * coin's denomination amount, respectively.
 */
int compare_denomination_desc(const void *a, const void *b) {
  const Coin *coinA = *(const Coin **)a;
  const Coin *coinB = *(const Coin **)b;
  return (coinB->denomination.amount > coinA->denomination.amount) -
         (coinB->denomination.amount < coinA->denomination.amount);
}

/**
 * @brief Generate coins for withdrawal.
 *
 * @param amount The target amount to withdraw.
 * @param time The current time in seconds.
 * @param default_wallet The wallet containing the default coins for generation.
 * @param num_coins Pointer to store the number of generated coins.
 * @return An array of generated coins.
 */
// TODO: Consider: This may generate withdraw coins even though their withdraw fee is higher/equal value.
Coin *generate_withdraw_coins(long long amount, long long time,
                              Wallet default_wallet, int *num_coins, long long *withdraw_fee,  int charge_fees) {
  if (!amount || !default_wallet.num_coins) { // if amount is 0, return
    return NULL;
  }
  // Temporary array for storing pointers to unique denominations in the default
  // wallet
  Coin **uniqueDenominations =
      malloc(sizeof(Coin *) * default_wallet.num_coins);
  int numUnique = 0;

  // Extract unique denominations (this example assumes all coins in
  // default_wallet are unique)
  for (int i = 0; i < default_wallet.num_coins; i++) {
    uniqueDenominations[numUnique++] = &default_wallet.coins[i];
  }

  // Sort denominations in descending order
  qsort(uniqueDenominations, numUnique, sizeof(Coin *),
        compare_denomination_desc);

  // printf("%lli Amount, max %lli. %i unique, %lu bytes, was %lu\n", amount, uniqueDenominations[0]->amount, numUnique, sizeof(Coin*) * numUnique, sizeof(Coin) * numUnique);

  // TODO: whats actually an upper bound here?
  Coin *generatedCoins = malloc(sizeof(Coin) * numUnique * 4); // In worst case, we use one of each denomination
  if (!generatedCoins) {
    free(uniqueDenominations);
    return NULL; // Allocation failed
  }

  int generatedCount = 0;
  long long remainingAmount = amount;
  for (int i = 0; i < numUnique && remainingAmount > 0; i++) {
      long long amount_including_fee = uniqueDenominations[i]->denomination.amount;
      if (charge_fees) {
          amount_including_fee += uniqueDenominations[i]->denomination.rules.fees.withdraw_fee.fee_satoshis;
          // exchange wont bill fee on remaining amount to avoid not beeing able to withdraw at all
          // this could lead to unexpected outcomes for different fee structures than the intended 1 satoshi
          if(i == numUnique - 1 && uniqueDenominations[i]->amount == uniqueDenominations[i]->denomination.rules.fees.withdraw_fee.fee_satoshis) {
              amount_including_fee -= uniqueDenominations[i]->denomination.rules.fees.withdraw_fee.fee_satoshis;
          }
      }
    while (remainingAmount >= amount_including_fee) {
        // printf("generatedCount: %i of denom: \n", generatedCount);
      // Create a coin of this denomination
      if(charge_fees){
        *withdraw_fee += amount_including_fee - uniqueDenominations[i]->denomination.amount;
      }
      generatedCoins[generatedCount] = *(uniqueDenominations[i]);
      generatedCoins[generatedCount].creation_timestamp = time;
      generatedCoins[generatedCount].uniqueId = nextUniqueId++;
      generatedCoins[generatedCount].amount = uniqueDenominations[i]->denomination.amount;
      generatedCoins[generatedCount].latest_recoup_time = 0l;
      remainingAmount -= amount_including_fee;
      generatedCount++;
    }
  }

  free(uniqueDenominations);

  // If after the loop, remainingAmount is not 0, it means the requested amount
  // cannot be exactly matched
  if (remainingAmount > 0) {
    free(generatedCoins);
    return NULL; // Indicate failure to generate the exact amount
  }

  // Optionally resize the generatedCoins array to the exact number of generated
  // coins
  Coin *resizedGeneratedCoins =
      realloc(generatedCoins, sizeof(Coin) * generatedCount);
  if (!resizedGeneratedCoins) {
    free(generatedCoins);
    return NULL; // Handle realloc failure (though this should be rare)
  }

  *num_coins = generatedCount;

  return resizedGeneratedCoins;
}

/**
 * @brief Add coins to the wallet.
 *
 * @param wallet Pointer to the wallet.
 * @param coins The array of coins to be added.
 * @param num_coins The number of coins to be added.
 */
void add_coins_to_wallet(Wallet *wallet, Coin *coins, int num_coins) {
  if (wallet == NULL || coins == NULL || num_coins <= 0) {
    return; // No operation if the input is invalid
  }

  // Calculate new size for the wallet's coins array
  int newSize = wallet->num_coins + num_coins;

  // Allocate a new array to hold both existing and new coins
  Coin *newCoinsArray = (Coin *)malloc(sizeof(Coin) * newSize);
  if (newCoinsArray == NULL) {
    printf("Memory allocation failed.\n");
    return;
  }

  if (wallet->num_coins > 0) {
    // Copy existing coins to the new array
    for (int i = 0; i < wallet->num_coins; i++) {
      newCoinsArray[i] = wallet->coins[i];
    }
    free(wallet->coins);
  }

  // Copy the new coins to the new array
  for (int i = 0; i < num_coins; i++) {
    if (!coins[i].uniqueId) {
      coins[i].uniqueId = nextUniqueId++;
    }
    newCoinsArray[wallet->num_coins + i] = coins[i];
  }

  free(coins);

  // Update the wallet
  wallet->coins = newCoinsArray;
  wallet->num_coins = newSize;
}

/**
 * @brief Remove selected coins from the wallet.
 *
 * @param wallet Pointer to the wallet.
 * @param coins The array of coins to be removed.
 * @param num_coins The number of coins to be removed.
 */
void remove_selected_coins(Wallet *wallet, Coin *coins, int num_coins) {
  if (wallet == NULL || wallet->num_coins == 0 || coins == NULL ||
      num_coins == 0) {
      printf("Error: remove_selected_coins called with invalid args\n");
    return; // No operation if the input is invalid
  }

  // Allocate a new array to hold the remaining coins
  Coin *remainingCoins = malloc(sizeof(Coin) * wallet->num_coins);
  if (remainingCoins == NULL) {
    printf("Memory allocation failed.\n");
    return;
  }

  int remainingCount = 0;

  // Iterate through the wallet's coins
  for (int i = 0; i < wallet->num_coins; i++) {
    int isRemoved = 0;

    // Check if the current coin is in the list of coins to be removed
    for (int j = 0; j < num_coins; j++) {
      if (wallet->coins[i].uniqueId == coins[j].uniqueId) {
        isRemoved = 1;
        break;
      }
    }
    
    // if(num_coins > 100){
    //   printf("\t\tremove coin: %lld: %d\n", wallet->coins[i].uniqueId, isRemoved);
    //   for(int i = 0; i < num_coins;i++){
    //     printf("%lld\n", wallet->coins[i].uniqueId);
    //   }
    // }

    // If the coin is not in the list of coins to be removed, add it to the
    // remaining coins
    if (!isRemoved) {
      remainingCoins[remainingCount++] = wallet->coins[i];
    }
  }

  free(coins);

  if(remainingCount == 0) {
      if(wallet->coins != NULL) {
        free(wallet->coins);
      }
      wallet->coins = NULL;
      wallet->num_coins = 0;
      return;
  }

  // Free the old coins array (if any)
  if (wallet->coins != NULL) {
    free(wallet->coins);
  }
    printf("remove_selected: called for %d coins, %d -> %d\n", num_coins, wallet->num_coins, remainingCount);

  remainingCoins = realloc(remainingCoins, sizeof(Coin) * remainingCount);
  // Update the wallet with the remaining coins
  wallet->coins = remainingCoins;
  wallet->num_coins = remainingCount;

}

/**
 * @brief Calculate the total fee for a set of coins based on the operation
 * type.
 *
 * @param coins The array of coins.
 * @param num_coins The number of coins.
 * @param operation The operation type.
 * @return The total fee for the specified operation.
 */
long long calculate_total_fee(Coin *coins, int num_coins,
                              operation_type operation) {
  long long totalFee = 0;

  for (int i = 0; i < num_coins; i++) {
    switch (operation) {
    case DEPOSIT_OP:
      totalFee += coins[i].denomination.rules.fees.deposit_fee.fee_satoshis;
      break;
    case REFUND_OP:
      totalFee += coins[i].denomination.rules.fees.refund_fee.fee_satoshis;
      break;
    case WITHDRAW_OP:
      totalFee += coins[i].denomination.rules.fees.withdraw_fee.fee_satoshis;
      break;
    case REFRESH_OP:
      totalFee += coins[i].denomination.rules.fees.refresh_fee.fee_satoshis;
      break;
    default:
      break;
    }
  }

  return totalFee;
}

long long calculate_total_fee_part(Coin *coins, int num_coins,
                                   operation_type operation) {
  long long totalFee = 0;

  for (int i = 0; i < num_coins; i++) {
    switch (operation) {
    case DEPOSIT_OP:
      totalFee += coins[i].denomination.rules.fees.deposit_fee.fee_satoshis;
      if (coins[i].denomination.amount != coins[i].amount) {
        totalFee += coins[i].denomination.rules.fees.refresh_fee.fee_satoshis;
      }
      break;
    case REFUND_OP:
      totalFee += coins[i].denomination.rules.fees.refund_fee.fee_satoshis;
      break;
    case WITHDRAW_OP:
      totalFee += coins[i].denomination.rules.fees.withdraw_fee.fee_satoshis;
      break;
    case REFRESH_OP:
      totalFee += coins[i].denomination.rules.fees.refresh_fee.fee_satoshis;
      break;
    default:
      break;
    }
  }

  return totalFee;
}

// TODO update descr
/**
 * @brief Calculate the renew fee for the wallet based on the current time.
 *
 * @param wallet The wallet containing the coins.
 * @param time The current time in seconds.
 * @return The total renew fee for the wallet.
 */
void refresh_old_coins(Wallet wallet, long long time, int* num_renewed_coins, long long *total_fee) {
    *num_renewed_coins = 0;
    *total_fee = 0;

    if(wallet.num_coins == 0){
        return;
    }

    // Coin *renew_coins = malloc(sizeof(Coin) * wallet.num_coins);
    for (int i = 0; i < wallet.num_coins; i++) {
        if (time > wallet.coins[i].denomination.rules.durations.deposit.time + wallet.coins[i].creation_timestamp) {
            long long value_before = wallet.coins[i].amount;
            long long renewFee = calculate_fee(wallet.coins[i], REFRESH_OP);
            // wallet.coins[i].creation_timestamp = time;
            if(wallet.coins[i].amount < renewFee){
                *total_fee += wallet.coins[i].amount; 
                wallet.coins[i].amount = 0;
            }else {
                wallet.coins[i].amount -= renewFee;
                *total_fee += renewFee;
            }
            (*num_renewed_coins)++;
            printf("refresh_old: %lld -> %lld\n", value_before, wallet.coins[i].amount);
        }
    }

  // if(!*num_renewed_coins) {
  //     free(renew_coins);
  //     return NULL;
  // }
  
  // renew_coins = realloc(renew_coins, sizeof(Coin) * *num_renewed_coins);

}

int is_dirty(Coin coin) { return coin.amount != coin.denomination.amount; }

void pprint(Coin *coins, int nr) {
  for (int i = 0; i < nr; i++) {
    Coin coin = coins[i];

    if (is_dirty(coin)) {
      printf("[%lld / %lld | D: %lld | R: %lld]", coin.amount,
             coin.denomination.amount,
             coin.denomination.rules.fees.deposit_fee.fee_satoshis,
             coin.denomination.rules.fees.refresh_fee.fee_satoshis);
    } else {
      printf("[%lld]", coin.amount);
    }
  }
  printf("\n");
}


/**
 * @brief Melt partially spent coins in wallet and add refreshed coins to it
 *
 * @param wallet
 * @param coins Coins to be checked
 * @param num_coins Number of coins in coins
 * @return
 */
void refresh_dirty_coins(Wallet *wallet, Wallet denomination_wallet, long long time, int save_for_recoup) {
  if (wallet == NULL) {
    return; // No operation if the input is invalid
  }

  long long amount_dirty = 0;
  int num_dirty_coins = 0;

  // Coin *spent_coins = (Coin *)malloc(sizeof(Coin*) * num_coins);
  Coin *dirty_coins = malloc(sizeof(Coin) * wallet->num_coins);

  long latest_deposit_expire = 0;

  for (int i = 0; i < wallet->num_coins; i++){
        if (wallet->coins[i].amount == wallet->coins[i].denomination.amount) {
            // spent_coins[num_spent_coins] = coins[i];
        }else if (0 <= wallet->coins[i].amount && wallet->coins[i].amount < wallet->coins[i].denomination.amount) {
            dirty_coins[num_dirty_coins] = wallet->coins[i];
            num_dirty_coins ++;
            amount_dirty += wallet->coins[i].amount;

    
            if(save_for_recoup) {
                latest_deposit_expire = max(latest_deposit_expire, wallet->coins[i].denomination.rules.durations.deposit.time);
            }
               // printf("\t melting %lld / %lld sum of change: %lld\n", wallet->coins[i].amount, wallet->coins[i].denomination.amount, amount_dirty);
        } else {
            // printf("ERROR: Coin has invalid amount %lld for denomination %lld\n", wallet->coins[i].amount, wallet->coins[i].denomination.amount);
            // exit(1);
        }
      
  }



  // spent_coins = realloc(spent_coins, num_spent_coins);
  // dirty_coins = realloc(dirty_coins, num_dirty_coins);
  int num_before = wallet->num_coins;
  // melt_selected_coins(wallet, dirty_coins, num_dirty_coins);
  dirty_coins = realloc(dirty_coins, sizeof(Coin) * num_dirty_coins);
  remove_selected_coins(wallet, dirty_coins, num_dirty_coins);
  int num_withdrawn = 0;
  // printf("withdraw from refresh: %lld\n", amount_dirty);
  Coin *withdrawn_coins = generate_withdraw_coins(amount_dirty, time, denomination_wallet, &num_withdrawn, NULL, FALSE);


  if(save_for_recoup) {
      for(int i = 0; i < num_withdrawn; i++) {
          withdrawn_coins[i].latest_recoup_time = latest_deposit_expire;
      }
  }

  add_coins_to_wallet(wallet, withdrawn_coins, num_withdrawn);

  printf("\t[%lld]     NUM DIRTY COINS: %d, NUM WITHDRAWN: %d, NUM BEFORE: %d, NUM AFTER: %d\n", time, num_dirty_coins, num_withdrawn, num_before, wallet->num_coins);
  if (num_before - num_dirty_coins + num_withdrawn != wallet->num_coins){
      printf(" Error: Mismatch\n");
  }
}

void spend_coin_selection(Coin* coins, int num_coins, Wallet *wallet) {
    int num_matched = 0;
    for (int i = 0; i < num_coins; i++) {
        for (int j = 0; j < wallet->num_coins; j++) {
            if(coins[i].uniqueId == wallet->coins[j].uniqueId) {
                printf("Spending: %lld with this coin[%lld]: %lld/%lld =>", coins[i].amount, coins[i].uniqueId, wallet->coins[j].amount, wallet->coins[j].denomination.amount);
                wallet->coins[j].amount -= coins[i].amount;
                printf("%lld\n", wallet->coins[j].amount);
                num_matched++;
            } 
        }
    }    

    if(num_matched != num_coins) {
        printf("Error: coins to spend could not be matched\n");
    }
}


// should return list of new + remaining coins and the refresh fee to be paid?
// TODO: generate new coins only from denoms possible in this scenario (pass
// denom wallet) void refresh_dirty_coins(Coin *coins, int num_coins) {
//
//     pprint(coins, num_coins);
//
//     return ;
//
//     long long total_refresh_fee = 0;
//
//     for(int i = 0; i < num_coins; i++) {
//         Coin coin_i = coins[i];
//         if (coin_i.amount < coin_i.denomination.amount) {
//             total_refresh_fee +=
//             coin_i.denomination.rules.fees.refresh_fee.fee_satoshis;
//         }
//     }
// }
//
//
