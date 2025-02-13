//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 04.03.2024.
// coin_selection.c
//

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdlib.h>
#include <stdio.h>
#include "coin_selection.h"


/// Global variable to generate unique IDs for coins
static long long nextUniqueId = 1;

static PyObject *pName, *pModule, *pFunc, *pValue, *kwargs;

int checkOrLoadPython() {

    PyStatus status;

    PyConfig config;

    if(pName != NULL){
        return 1;
    }

    PyConfig_InitPythonConfig(&config);

    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        printf("init error");
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
            printf("Failed to load Python");
            return 0;
        }
        return 1;        
    }
    return 0;
}

/**
 * @brief Calculate the score of a coin based on its expiration time and denomination.
 *
 * @param coin The coin whose score is to be calculated.
 * @param currentTime The current time in seconds.
 * @param maxDenom The maximum denomination value.
 * @param minDenom The minimum denomination value.
 * @return The calculated score of the coin.
 */
double calculate_coin_score(Coin *coin, long long currentTime, long long maxDenom, long long minDenom) {

    const double MAX_SCORE = 100.0;
    const double WEIGHT_TIME = 0.8;
    const double WEIGHT_DENOM = 0.2;

    // Time to expiration score calculation
    long long timeLeft = (coin->creation_timestamp + coin->denomination.rules.durations.legal.time) - currentTime;
    long long maxTime = coin->denomination.rules.durations.legal.time;
    double scoreTime = MAX_SCORE * ((double)timeLeft / (double)maxTime);

    // Denomination score calculation
    double normalizedDenom = (double)(coin->denomination.amount - minDenom) / (double)(maxDenom - minDenom);
    double scoreDenom = MAX_SCORE * (1.0 - normalizedDenom);

    // Combine scores with weights
    double finalScore = (WEIGHT_TIME * scoreTime) + (WEIGHT_DENOM * scoreDenom);

    return finalScore;
}

/**
 * @brief Comparison function for sorting coins by creation timestamp in ascending order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first coin's creation timestamp
 *         is less than, equal to, or greater than the second coin's creation timestamp, respectively.
 */
int compare_creation_time_asc(const void *a, const void *b) {
    const Coin *coinA = (const Coin *)a;
    const Coin *coinB = (const Coin *)b;
    return (coinA->creation_timestamp > coinB->creation_timestamp) - (coinA->creation_timestamp < coinB->creation_timestamp);
}

/**
 * @brief Comparison function for sorting denominations in ascending order.
 *
 * @param a Pointer to the first denomination.
 * @param b Pointer to the second denomination.
 * @return An integer less than, equal to, or greater than zero if the first denomination
 *         is less than, equal to, or greater than the second denomination, respectively.
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
 * @return An integer less than, equal to, or greater than zero if the first denomination
 *         is greater than, equal to, or less than the second denomination, respectively.
 */
int compare_denomination_desc_ll(const void *a, const void *b) {
    long long denomA = *(const long long *)a;
    long long denomB = *(const long long *)b;
    return (denomB > denomA) - (denomB < denomA);
}

/**
 * @brief Comparison function for sorting coin wrappers by score in descending order.
 *
 * @param a Pointer to the first coin wrapper.
 * @param b Pointer to the second coin wrapper.
 * @return An integer less than, equal to, or greater than zero if the first coin wrapper's score
 *         is less than, equal to, or greater than the second coin wrapper's score, respectively.
 */
int compare_coin_wrappers(const void *a, const void *b) {
    const coin_wrapper *wrapperA = (const coin_wrapper *)a;
    const coin_wrapper *wrapperB = (const coin_wrapper *)b;
    if (wrapperA->score < wrapperB->score) return 1; // Descending order
    if (wrapperA->score > wrapperB->score) return -1;
    return 0;
}

/**
 * @brief Comparison function for sorting coins by denomination amount in descending order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first coin's denomination amount
 *         is greater than, equal to, or less than the second coin's denomination amount, respectively.
 */
int compare_coins_desc(const void *a, const void *b) {
    Coin *coinA = (Coin *)a;
    Coin *coinB = (Coin *)b;
    return (coinB->denomination.amount - coinA->denomination.amount);
}

/**
 * @brief Comparison function for sorting coins by denomination amount in ascending order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first coin's denomination amount
 *         is less than, equal to, or greater than the second coin's denomination amount, respectively.
 */
int compare_coins_asc(const void *a, const void *b) {
    Coin *coinA = (Coin *)a;
    Coin *coinB = (Coin *)b;
    return -(coinB->denomination.amount - coinA->denomination.amount);
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
Coin* allocate_max_bills(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount) {
    Coin* coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
    if (coinsCopy == NULL) return NULL;
    for (int i = 0; i < wallet.num_coins; i++) {
        coinsCopy[i] = wallet.coins[i];
    }

    // Sort coins in descending order based on their amount
    qsort(coinsCopy, wallet.num_coins, sizeof(Coin), compare_coins_desc);

    long long amount_collected = 0;
    int selectedCount = 0;
    for (int j = 0; j < wallet.num_coins && amount_collected < amount; j++) {
        amount_collected += coinsCopy[j].denomination.amount;
        selectedCount++;
    }

    *allocated_amount = amount_collected;

    // Allocate memory for selected coins
    Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
    if (selectedCoins == NULL) {
        free(coinsCopy);
        return NULL;
    }

    for (int k = 0; k < selectedCount; k++) {
        selectedCoins[k] = coinsCopy[k];
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
Coin* allocate_min_bills(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount) {
    Coin* coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
    if (coinsCopy == NULL) return NULL;
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

    *allocated_amount = amount_collected;

    // Allocate memory for selected coins
    Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
    if (selectedCoins == NULL) {
        free(coinsCopy);
        return NULL; // Allocation failed
    }

    // Copy selected coins
    for (int k = 0; k < selectedCount; k++) {
        selectedCoins[k] = coinsCopy[k];
    }

    *num_allocated_coins = selectedCount; // Update the number of allocated coins

    free(coinsCopy);
    return selectedCoins;
}

/**
 * @brief Comparison function for sorting coins by expiration time, then by denomination amount.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first coin's expiration time
 *         is less than, equal to, or greater than the second coin's expiration time, respectively.
 *         If the expiration times are equal, it compares by denomination amount.
 */
int compare_expiry_time_amount(const void *a, const void *b) {
    const Coin *coinA = (const Coin *)a;
    const Coin *coinB = (const Coin *)b;

    // First, compare by expiration time
    if (coinA->creation_timestamp + coinA->denomination.rules.durations.legal.time < coinB->creation_timestamp + coinB->denomination.rules.durations.legal.time) {
        return -1;
    } else if (coinA->creation_timestamp + coinA->denomination.rules.durations.legal.time > coinB->creation_timestamp + coinB->denomination.rules.durations.legal.time) {
        return 1;
    }

    // If expiration times are equal, compare by denomination amount
    return (coinA->denomination.amount > coinB->denomination.amount) - (coinA->denomination.amount < coinB->denomination.amount);
}

/**
 * @brief Allocate coins from the wallet that are closest to expiration and minimize the number of bills used.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin* allocate_closest_to_expire_min_bills(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount) {
    // Allocate memory to copy coins
    Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
    if (coinsCopy == NULL) return NULL; // Check if malloc failed

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

    *allocated_amount = amount_collected;

    // Allocate memory for selected coins
    Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
    if (selectedCoins == NULL) {
        free(coinsCopy);
        return NULL; // Allocation failed
    }

    // Copy selected coins
    for (int k = 0; k < selectedCount; k++) {
        selectedCoins[k] = coinsCopy[k];
    }

    *num_allocated_coins = selectedCount; // Update the number of allocated coins

    free(coinsCopy);
    return selectedCoins; // Return the array of selected coins
}

/**
 * @brief Comparison function for sorting coins by expiration time, then by denomination amount in reverse order.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first coin's expiration time
 *         is less than, equal to, or greater than the second coin's expiration time, respectively.
 *         If the expiration times are equal, it compares by denomination amount in reverse order.
 */
int compare_expiry_time_amount_reverse(const void *a, const void *b) {
    const Coin *coinA = (const Coin *)a;
    const Coin *coinB = (const Coin *)b;

    // First, compare by expiration time
    if (coinA->creation_timestamp + coinA->denomination.rules.durations.legal.time < coinB->creation_timestamp + coinB->denomination.rules.durations.legal.time) {
        return -1;
    } else if (coinA->creation_timestamp + coinA->denomination.rules.durations.legal.time > coinB->creation_timestamp + coinB->denomination.rules.durations.legal.time) {
        return 1;
    }

    // If expiration times are equal, compare by denomination amount in reverse order
    return (coinB->denomination.amount > coinA->denomination.amount) - (coinB->denomination.amount < coinA->denomination.amount);
}

/**
 * @brief Allocate coins from the wallet that are closest to expiration and maximize the number of bills used.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin* allocate_closest_to_expire_max_bills(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount) {
    // Allocate memory to copy coins
    Coin *coinsCopy = malloc(sizeof(Coin) * wallet.num_coins);
    if (coinsCopy == NULL) return NULL; // Check if malloc failed

    // Copy coins from the wallet
    for (int i = 0; i < wallet.num_coins; i++) {
        coinsCopy[i] = wallet.coins[i];
    }

    // Sort coins by expiration time, then by amount in reverse order
    qsort(coinsCopy, wallet.num_coins, sizeof(Coin), compare_expiry_time_amount_reverse);

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

    *allocated_amount = amount_collected;

    // Allocate memory for selected coins
    Coin *selectedCoins = malloc(sizeof(Coin) * selectedCount);
    if (selectedCoins == NULL) {
        free(coinsCopy);
        return NULL; // Allocation failed
    }

    // Copy selected coins
    for (int k = 0; k < selectedCount; k++) {
        selectedCoins[k] = coinsCopy[k];
    }

    *num_allocated_coins = selectedCount; // Update the number of allocated coins

    free(coinsCopy);
    return selectedCoins; // Return the array of selected coins
}

/**
 * @brief Allocate coins from the wallet randomly until the desired amount is reached.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @return An array of allocated coins.
 */
Coin* allocate_random_bills(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount) {

    // Create an array of indices representing the coins
    int *indices = malloc(sizeof(int) * wallet.num_coins);
    if (indices == NULL) return NULL; // Check if malloc failed for indices

    for (int i = 0; i < wallet.num_coins; i++) {
        indices[i] = i; // Initialize indices with the coin positions
    }

    long long amount_collected = 0;
    Coin *selectedCoins = malloc(sizeof(Coin) * wallet.num_coins); // Allocate memory to store potentially all coins
    if (selectedCoins == NULL) {
        free(indices);
        return NULL; // Allocation failed
    }

    int selectedCount = 0;
    int remainingCoins = wallet.num_coins;

    while (amount_collected < amount && remainingCoins > 0) {
        int randIndex = rand() % remainingCoins; // Pick a random index from the remaining indices
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

    *allocated_amount = amount_collected;

    // Resize the selectedCoins array to the actual number of selected coins
    Coin *finalSelectedCoins = realloc(selectedCoins, sizeof(Coin) * selectedCount);
    if (finalSelectedCoins == NULL) {
        free(indices);
        free(selectedCoins);
        return NULL;
    }

    *num_allocated_coins = selectedCount;

    free(indices);
    return finalSelectedCoins;
}

/**
 * @brief Allocate coins from the wallet to maximize the number of bills used, weighted by time to expiration.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param currentTime The current time in seconds.
 * @return An array of allocated coins.
 */
Coin* allocate_max_bills_time_to_expire_weighted(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount, long long currentTime) {
    coin_wrapper *wrappers = (coin_wrapper *)malloc(sizeof(coin_wrapper) * wallet.num_coins);
    if (!wrappers) return NULL;

    long long maxDenom = -100;
    long long minDenom = 1000000000;

    for (int i = 0; i < wallet.num_coins; ++i) {
        if(maxDenom > wallet.coins[i].denomination.amount){
            maxDenom = wallet.coins[i].denomination.amount;
        }
        if(minDenom < wallet.coins[i].denomination.amount){
            minDenom = wallet.coins[i].denomination.amount;
        }
    }

    // Populate wrappers and compute scores
    for (int i = 0; i < wallet.num_coins; ++i) {
        wrappers[i].coin = &wallet.coins[i];
        wrappers[i].score = calculate_coin_score(wrappers[i].coin, currentTime, maxDenom, minDenom);
    }

    // Sort the wrappers by score in descending order
    qsort(wrappers, wallet.num_coins, sizeof(coin_wrapper), compare_coin_wrappers);

    // Allocate and select coins based on sorted wrappers until the desired amount is reached
    *allocated_amount = 0;
    *num_allocated_coins = 0;
    for (int i = 0; i < wallet.num_coins && *allocated_amount < amount; ++i) {
        *allocated_amount += wrappers[i].coin->denomination.amount;
        (*num_allocated_coins)++;
    }

    // Allocate memory for selected coins and copy them
    Coin *selectedCoins = (Coin *)malloc(sizeof(Coin) * (*num_allocated_coins));
    if (!selectedCoins) {
        free(wrappers);
        return NULL;
    }
    for (int i = 0; i < *num_allocated_coins; ++i) {
        selectedCoins[i] = *wrappers[i].coin;
    }

    free(wrappers);
    return selectedCoins;
}


/**
 * @brief Allocate coins from the wallet evenly from the smallest to the largest denomination.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination information.
 * @return An array of allocated coins.
 */
Coin* allocate_coins_even_from_min_to_max(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount, Wallet denomination_wallet){
    // Sort coins in the wallet by their creation timestamp in ascending order
    qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_creation_time_asc);

    // Allocate memory for the 2D array
    int num_denominations = denomination_wallet.num_coins;
    long long **denom_array = malloc(2 * sizeof(long long *));
    if (denom_array == NULL) return NULL; // Check if malloc failed

    denom_array[0] = malloc(num_denominations * sizeof(long long)); // For denominations
    denom_array[1] = malloc(num_denominations * sizeof(long long)); // For quantities
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
    qsort(denom_array[0], num_denominations, sizeof(long long), compare_denomination_asc);

    // Update the quantity array with the actual quantities of the denominations from the wallet
    for (int i = 0; i < wallet.num_coins; i++) {
        for (int j = 0; j < num_denominations; j++) {
            if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
                denom_array[1][j]++;
                break;
            }
        }
    }

    // Allocate memory for selected coins
    Coin* selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
    if (selectedCoins == NULL) {
        free(denom_array[0]);
        free(denom_array[1]);
        free(denom_array);
        return NULL; // Allocation failed
    }

    long long amount_collected = 0;
    int selectedCount = 0;
    int *selectedFlags = malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
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
                    if (wallet.coins[j].denomination.amount == denom_array[0][i] && selectedFlags[j] == 0) {
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

    *allocated_amount = amount_collected;
    *num_allocated_coins = selectedCount;

    // Resize the selectedCoins array to the actual number of selected coins
    Coin *finalSelectedCoins = realloc(selectedCoins, sizeof(Coin) * selectedCount);
    if (finalSelectedCoins == NULL) {
        // If realloc failed, free original block and return NULL
        free(selectedCoins);
        free(denom_array[0]);
        free(denom_array[1]);
        free(denom_array);
        free(selectedFlags);
        return NULL;
    }


    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    free(selectedFlags);

    return finalSelectedCoins;
}


/**
 * @brief Allocate coins from the wallet evenly from the largest to the smallest denomination.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination information.
 * @return An array of allocated coins.
 */
Coin* allocate_coins_even_from_max_to_min(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount, Wallet denomination_wallet){
    // Sort coins in the wallet by their creation timestamp in ascending order
    qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_creation_time_asc);

    // Allocate memory for the 2D array
    int num_denominations = denomination_wallet.num_coins;
    long long **denom_array = malloc(2 * sizeof(long long *));
    if (denom_array == NULL) return NULL; // Check if malloc failed

    denom_array[0] = malloc(num_denominations * sizeof(long long)); // For denominations
    denom_array[1] = malloc(num_denominations * sizeof(long long)); // For quantities
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
    qsort(denom_array[0], num_denominations, sizeof(long long), compare_denomination_desc_ll);

    // Update the quantity array with the actual quantities of the denominations from the wallet
    for (int i = 0; i < wallet.num_coins; i++) {
        for (int j = 0; j < num_denominations; j++) {
            if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
                denom_array[1][j]++;
                break;
            }
        }
    }

    // Allocate memory for selected coins
    Coin* selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
    if (selectedCoins == NULL) {
        free(denom_array[0]);
        free(denom_array[1]);
        free(denom_array);
        return NULL; // Allocation failed
    }

    long long amount_collected = 0;
    int selectedCount = 0;
    int *selectedFlags = malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
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
                    if (wallet.coins[j].denomination.amount == denom_array[0][i] && selectedFlags[j] == 0) {
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

    *allocated_amount = amount_collected;
    *num_allocated_coins = selectedCount;

    // Resize the selectedCoins array to the actual number of selected coins
    Coin *finalSelectedCoins = realloc(selectedCoins, sizeof(Coin) * selectedCount);
    if (finalSelectedCoins == NULL) {
        // If realloc failed, free original block and return NULL
        free(selectedCoins);
        free(denom_array[0]);
        free(denom_array[1]);
        free(denom_array);
        free(selectedFlags);
        return NULL;
    }

    // Clean up
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    free(selectedFlags);

    return finalSelectedCoins;
}

/**
 * @brief Allocate coins from the wallet using a greedy algorithm from the smallest to the largest denomination.
 *
 * @param wallet The wallet containing the coins.
 * @param amount The target amount to allocate.
 * @param num_allocated_coins Pointer to store the number of allocated coins.
 * @param allocated_amount Pointer to store the total allocated amount.
 * @param denomination_wallet The wallet containing the denomination information.
 * @return An array of allocated coins.
 */
Coin* allocate_coins_greedy_min_to_max(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount, Wallet denomination_wallet){
    // Sort coins in the wallet by their creation timestamp in ascending order
    qsort(wallet.coins, wallet.num_coins, sizeof(Coin), compare_creation_time_asc);

    // Allocate memory for the 2D array
    int num_denominations = denomination_wallet.num_coins;
    long long **denom_array = malloc(2 * sizeof(long long *));
    if (denom_array == NULL) return NULL; // Check if malloc failed

    denom_array[0] = malloc(num_denominations * sizeof(long long)); // For denominations
    denom_array[1] = malloc(num_denominations * sizeof(long long)); // For quantities
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
    qsort(denom_array[0], num_denominations, sizeof(long long), compare_denomination_desc_ll);

    // Update the quantity array with the actual quantities of the denominations from the wallet
    for (int i = 0; i < wallet.num_coins; i++) {
        for (int j = 0; j < num_denominations; j++) {
            if (wallet.coins[i].denomination.amount == denom_array[0][j]) {
                denom_array[1][j]++;
                break;
            }
        }
    }

    // Allocate memory for selected coins
    Coin* selectedCoins = malloc(sizeof(Coin) * wallet.num_coins);
    if (selectedCoins == NULL) {
        free(denom_array[0]);
        free(denom_array[1]);
        free(denom_array);
        return NULL; // Allocation failed
    }

    long long amount_collected = 0;
    int selectedCount = 0;
    int *selectedFlags = malloc(sizeof(int) * wallet.num_coins); // Flags to mark selected coins
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

        // Find the coin that brings us closest to the target amount without exceeding it
        for (int i = 0; i < wallet.num_coins; i++) {
            if (selectedFlags[i] == 0) {
                if(first){
                    closestAmount = wallet.coins[i].denomination.amount;
                    closestIndex = i;
                    first = 0;
                }
                long long tempAmount = amount_collected + wallet.coins[i].denomination.amount;
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

    *allocated_amount = amount_collected;
    *num_allocated_coins = selectedCount;

    // Resize the selectedCoins array to the actual number of selected coins
    Coin *finalSelectedCoins = realloc(selectedCoins, sizeof(Coin) * selectedCount);
    if (finalSelectedCoins == NULL) {
        // If realloc failed, free original block and return NULL
        free(selectedCoins);
        free(denom_array[0]);
        free(denom_array[1]);
        free(denom_array);
        free(selectedFlags);
        return NULL;
    }

    // Clean up
    free(denom_array[0]);
    free(denom_array[1]);
    free(denom_array);
    free(selectedFlags);

    return finalSelectedCoins;
}

PyObject* encodeFee(Fee fee){
    return Py_BuildValue("(L,f)", fee.fee_satoshis, fee.percentage_fee); 
}

PyObject* encodeFees(Fees fees){
    return Py_BuildValue("(O,O,O,O)",
            encodeFee(fees.deposit_fee),
            encodeFee(fees.refund_fee),
            encodeFee(fees.withdraw_fee),
            encodeFee(fees.refresh_fee)
            );   
}
PyObject* encodeDurations(Durations durations){
   return Py_BuildValue("(L,L,L)", durations.legal.time, durations.deposit.time, durations.withdraw.time); 
}

PyObject* encodeRules(Rules rules){
   return Py_BuildValue("(i,s,O,O)", rules.rsa_keysize, rules.cipher, encodeFees(rules.fees), encodeDurations(rules.durations)); 
}

PyObject* encodeDenomination(Denomination denom){
   return Py_BuildValue("(s,L,O)", denom.name, denom.amount, encodeRules(denom.rules)); 
}

PyObject* encodeCoin(Coin coin){
    return Py_BuildValue("(L,O,L)", coin.uniqueId, encodeDenomination(coin.denomination), coin.creation_timestamp);
}

PyObject* encodeGlobalFees(GlobalFees global_fees){
    return Py_BuildValue("(O,O)", encodeFee(global_fees.wire_fee), encodeFee(global_fees.closing_fee)); 
}

PyObject* encodeWallet(Wallet wallet){
    // Coin* coins
    // int num_coins
    // GlobalFees globalFees
    PyObject *walletObj, *coins, *globalFees;

    walletObj = PyDict_New();
    coins = PyList_New(0);

    for(int i = 0;i < wallet.num_coins;++i){
        PyObject *coin = encodeCoin(wallet.coins[i]);
        PyList_Append(coins, coin);
    }
    PyDict_SetItemString(walletObj, "coins", coins);
    PyDict_SetItemString(walletObj, "global_fees", encodeGlobalFees(wallet.global_fees));
    return walletObj; 
}

inline long min(long a, long b) { return (a < b) ? a : b; }

Coin* allocate_call_external(Wallet wallet, long long amount, int* num_allocated_coins, long long* allocated_amount, Wallet denomination_wallet){

    PyObject *pValue, *kwargs, *pAmount;
    if (checkOrLoadPython()) {
        
        kwargs = PyDict_New();
        
        pAmount = PyLong_FromLong(amount);

        printf("coins: %i\n", wallet.num_coins);

        PyDict_SetItemString(kwargs, "amount", pAmount);
        PyDict_SetItemString(kwargs, "wallet", encodeWallet(wallet));
        pValue = PyObject_CallOneArg(pFunc, kwargs);

        PyErr_Print();
        Py_DECREF(kwargs);
        Py_DECREF(pAmount);
         
        // Allocate memory for selected coins
        Coin *selectedCoins = malloc(sizeof(Coin) * PyList_Size(pValue));
        if (selectedCoins == NULL) {
            return NULL; // Allocation failed
        }
        
        long long totalAmount = 0;
        // Copy selected coins
        for (int k = 0; k < PyList_Size(pValue); k++) {
            long coinAmount = wallet.coins[PyLong_AsInt(PyList_GetItem(pValue, k))].denomination.amount;
            printf("index %i selected for %i\n", PyLong_AsInt(PyList_GetItem(pValue, k)), coinAmount);
            selectedCoins[k] = wallet.coins[PyLong_AsInt(PyList_GetItem(pValue, k))];
            totalAmount += coinAmount;
        }
        *allocated_amount = min(totalAmount, amount);
        *num_allocated_coins = PyList_Size(pValue);

        if (pValue != NULL) {
            Py_DECREF(pValue);
        }else {
            printf("NULL result\n"); 
        }
        return selectedCoins;
    }else {
        printf("Python didnt load :/\n");
        return NULL;    
    }
    return NULL;
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
 * @param denomination_wallet The wallet containing the denomination information.
 * @return An array of allocated coins.
 */
Coin* allocate_coins_for_deposit(Wallet wallet, long long amount, strategy strategy, long long time, int* num_allocated_coins, long long* allocated_amount, Wallet denomination_wallet){

    if (!wallet.num_coins || !amount) { // if wallet is empty or amount 0, return
        return NULL;
    }
    switch (strategy) {
        case MAX_BILLS:
            return allocate_max_bills(wallet, amount, num_allocated_coins, allocated_amount);
        case MIN_BILLS:
            return allocate_min_bills(wallet, amount, num_allocated_coins, allocated_amount);
        case CLOSEST_TO_EXPIRE_MIN_BILLS:
            return allocate_closest_to_expire_min_bills(wallet, amount, num_allocated_coins, allocated_amount);
        case CLOSEST_TO_EXPIRE_MAX_BILLS:
            return allocate_closest_to_expire_max_bills(wallet, amount, num_allocated_coins, allocated_amount);
        case MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED:
            return allocate_max_bills_time_to_expire_weighted(wallet, amount, num_allocated_coins, allocated_amount, time);
        case RANDOM:
            return allocate_random_bills(wallet, amount, num_allocated_coins, allocated_amount);
        case EVEN_FROM_MIN_TO_MAX:
            return allocate_coins_even_from_min_to_max(wallet, amount, num_allocated_coins, allocated_amount, denomination_wallet);
        case EVEN_FROM_MAX_TO_MIN:
            return allocate_coins_even_from_max_to_min(wallet, amount, num_allocated_coins, allocated_amount, denomination_wallet);
        case GREEDY_MIN_TO_MAX:
            return allocate_coins_greedy_min_to_max(wallet, amount, num_allocated_coins, allocated_amount, denomination_wallet);
        case CUSTOM_EXTERNAL:
            return allocate_call_external(wallet, amount, num_allocated_coins, allocated_amount, denomination_wallet);
        default:
            return allocate_random_bills(wallet, amount, num_allocated_coins, allocated_amount);
    }
}

/**
 * @brief Comparison function for sorting coins in descending order by denomination amount.
 *
 * @param a Pointer to the first coin.
 * @param b Pointer to the second coin.
 * @return An integer less than, equal to, or greater than zero if the first coin's denomination amount
 *         is greater than, equal to, or less than the second coin's denomination amount, respectively.
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
Coin* generate_withdraw_coins(long long amount, long long time, Wallet default_wallet, int *num_coins) {
    if (!amount || !default_wallet.num_coins) { // if amount is 0, return
        return NULL;
    }
    // Temporary array for storing pointers to unique denominations in the default wallet
    Coin **uniqueDenominations = malloc(sizeof(Coin*) * default_wallet.num_coins);
    int numUnique = 0;

    // Extract unique denominations (this example assumes all coins in default_wallet are unique)
    for (int i = 0; i < default_wallet.num_coins; i++) {
        uniqueDenominations[numUnique++] = &default_wallet.coins[i];
    }

    // Sort denominations in descending order
    qsort(uniqueDenominations, numUnique, sizeof(Coin*), compare_denomination_desc);

    Coin *generatedCoins = malloc(sizeof(Coin) * numUnique); // In worst case, we use one of each denomination
    if (!generatedCoins) {
        free(uniqueDenominations);
        return NULL; // Allocation failed
    }

    int generatedCount = 0;
    long long remainingAmount = amount;
    for (int i = 0; i < numUnique && remainingAmount > 0; i++) {
        while (remainingAmount >= uniqueDenominations[i]->denomination.amount) {
            // Create a coin of this denomination
            generatedCoins[generatedCount] = *(uniqueDenominations[i]);
            generatedCoins[generatedCount].creation_timestamp = time;
            generatedCoins[generatedCount].uniqueId = nextUniqueId++;
            remainingAmount -= uniqueDenominations[i]->denomination.amount;
            generatedCount++;
        }
    }

    free(uniqueDenominations);

    // If after the loop, remainingAmount is not 0, it means the requested amount cannot be exactly matched
    if (remainingAmount > 0) {
        free(generatedCoins);
        return NULL; // Indicate failure to generate the exact amount
    }

    // Optionally resize the generatedCoins array to the exact number of generated coins
    Coin *resizedGeneratedCoins = realloc(generatedCoins, sizeof(Coin) * generatedCount);
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
void add_coins_to_wallet(Wallet* wallet, Coin* coins, int num_coins) {
    if (wallet == NULL || coins == NULL || num_coins <= 0) {
        return; // No operation if the input is invalid
    }

    // Calculate new size for the wallet's coins array
    int newSize = wallet->num_coins + num_coins;

    // Allocate a new array to hold both existing and new coins
    Coin* newCoinsArray = (Coin*)malloc(sizeof(Coin) * newSize);
    if (newCoinsArray == NULL) {
        printf("Memory allocation failed.\n");
        return;
    }

    if(wallet->num_coins> 0) {
        // Copy existing coins to the new array
        for (int i = 0; i < wallet->num_coins; i++) {
            newCoinsArray[i] = wallet->coins[i];
        }
        free(wallet->coins);
    }

    // Copy the new coins to the new array
    for (int i = 0; i < num_coins; i++) {
        if(!coins[i].uniqueId){
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
void remove_selected_coins(Wallet* wallet, Coin* coins, int num_coins) {
    if (wallet == NULL || wallet->num_coins == 0 || coins == NULL || num_coins == 0) {
        return; // No operation if the input is invalid
    }

    // Allocate a new array to hold the remaining coins
    Coin* remainingCoins = (Coin*)malloc(sizeof(Coin) * (wallet->num_coins - num_coins));
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

        // If the coin is not in the list of coins to be removed, add it to the remaining coins
        if (!isRemoved) {
            remainingCoins[remainingCount++] = wallet->coins[i];
        }
    }

    // Free the old coins array (if any)
    if (wallet->coins != NULL) {
        free(wallet->coins);
        wallet->coins = NULL;
    }

    free(coins);

    // Update the wallet with the remaining coins
    wallet->coins = remainingCoins;
    wallet->num_coins = remainingCount;
}

/**
 * @brief Calculate the total fee for a set of coins based on the operation type.
 *
 * @param coins The array of coins.
 * @param num_coins The number of coins.
 * @param operation The operation type.
 * @return The total fee for the specified operation.
 */
long long calculate_total_fee(Coin* coins, int num_coins, operation_type operation) {
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

/**
 * @brief Calculate the renew fee for the wallet based on the current time.
 *
 * @param wallet The wallet containing the coins.
 * @param time The current time in seconds.
 * @return The total renew fee for the wallet.
 */
long long calculate_renew_fee(Wallet wallet, long long time) {
    long long totalRenewFee = 0;

    for (int i = 0; i < wallet.num_coins; i++) {
        long long timeDifference = time - wallet.coins[i].creation_timestamp;
        if(timeDifference > wallet.coins[i].denomination.rules.durations.legal.time) {
            long long renewFee = calculate_fee(wallet.coins[i], REFRESH_OP);
            wallet.coins[i].creation_timestamp = time;
            totalRenewFee += renewFee;
        }
    }

    return totalRenewFee;
}
