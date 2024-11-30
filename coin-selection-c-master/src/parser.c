//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 10.03.2024.
// parser.c
//

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "cjson/cJSON.h"

// Define constants for file reading
#define MAX_LINE_LENGTH 1024
#define MAX_SECTIONS 100
#define MAX_BLOCK_SIZE (1024 * 100)

/**
 * @brief Convert a time string to seconds.
 *
 * @param time_str The time string to convert.
 * @return The number of seconds represented by the time string, or -1 if the format is invalid.
 */
long long time_to_seconds(const char* time_str) {
    long long seconds;
    long long value = 0;
    char unit[20]; // Assumes the longest time unit string is less than 20 characters

    // Parse the time_str to get the numeric value and the time unit
    if (sscanf(time_str, "%lld %19s", &value, unit) < 2) {
        // Handle error if the format of time_str doesn't match the expected pattern
        fprintf(stderr, "Invalid time format: %s\n", time_str);
        return -1; // Returning -1 to indicate an error
    }

    // Convert the time unit to lowercase for easier comparison
    for(int i = 0; unit[i]; i++){
        unit[i] = tolower((unsigned char) unit[i]);
    }

    // Determine the unit of time and calculate the number of seconds
    if (strcmp(unit, "year") == 0 || strcmp(unit, "years") == 0) {
        seconds = value * 365 * 24 * 60 * 60; // Simplified, not accounting for leap years
    } else if (strcmp(unit, "week") == 0 || strcmp(unit, "weeks") == 0) {
        seconds = value * 7 * 24 * 60 * 60;
    } else if (strcmp(unit, "day") == 0 || strcmp(unit, "days") == 0) {
        seconds = value * 24 * 60 * 60;
    } else if (strcmp(unit, "hour") == 0 || strcmp(unit, "hours") == 0) {
        seconds = value * 60 * 60;
    } else if (strcmp(unit, "minute") == 0 || strcmp(unit, "minutes") == 0) {
        seconds = value * 60;
    } else if (strcmp(unit, "second") == 0 || strcmp(unit, "seconds") == 0) {
        seconds = value;
    } else {
        // Handle unrecognized unit
        fprintf(stderr, "Unrecognized time unit: %s\n", unit);
        return -1; // Returning -1 to indicate an error
    }

    return seconds;
}

/**
 * @brief Parse a number from a string, removing any decimal points.
 *
 * @param input The input string to parse.
 * @return The parsed number as a long long, or -1 on failure.
 */
long long parse_number(const char* input) {
    // Find the position of the colon in the input string
    const char* colonPos = strchr(input, ':');
    if (!colonPos) {
        fprintf(stderr, "Invalid input format. Colon ':' not found.\n");
        return -1; // Indicate failure
    }

    // Move past the colon to the start of the numeric part
    const char* numericPart = colonPos + 1;

    // Check if there is a decimal point
    const char* decimalPointPos = strchr(numericPart, '.');
    char* parsedNumber;

    if (decimalPointPos) {
        // Create a copy of the numeric part and remove the decimal point
        parsedNumber = strdup(numericPart);
        if (!parsedNumber) {
            fprintf(stderr, "Memory allocation failed.\n");
            return -1; // Indicate failure
        }
        // Remove the decimal point by shifting the rest of the string left
        memmove(parsedNumber + (decimalPointPos - numericPart), decimalPointPos + 1, strlen(decimalPointPos));
    } else {
        // If there is no decimal point, just copy the numeric part
        parsedNumber = strdup(numericPart);
        if (!parsedNumber) {
            fprintf(stderr, "Memory allocation failed.\n");
            return -1; // Indicate failure
        }
    }

    // Convert the numeric part to a long long
    long long amount = atoll(parsedNumber);
    free(parsedNumber); // Free the allocated memory

    return amount;
}

/**
 * @brief Trim whitespace from the beginning and end of a string.
 *
 * @param str The string to trim.
 * @return The trimmed string.
 */
char* trim_whitespace(char *str) {
    char *end;

    while(isspace((unsigned char)*str)) {
        str++;
    }

    if(*str == 0) {
        return str;
    }

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) {
        end--;
    }

    *(end + 1) = '\0';

    return str;
}

/**
 * @brief Parse the currency name from the [taler] block in the configuration file.
 *
 * @param config_data The configuration data as a string.
 * @return The parsed currency name, or NULL if not found.
 */
char* parse_currency_name(const char* config_data) {
    const char* taler_block_start = strstr(config_data, "[taler]");
    if (taler_block_start == NULL) {
        return NULL;
    }

    const char* line_start = taler_block_start;
    while ((line_start = strstr(line_start, "\n")) != NULL) {
        // Move past the newline character
        line_start++;

        // Find the next newline to isolate the line
        const char* line_end = strstr(line_start, "\n");
        if (line_end == NULL) {
            // End of the file reached without finding a complete line
            break;
        }

        // Copy the line into a temporary buffer
        size_t line_length = line_end - line_start;
        char* line_buffer = malloc(line_length + 1);
        strncpy(line_buffer, line_start, line_length);
        line_buffer[line_length] = '\0';

        // Check if the line contains the currency keyword
        if (strncmp(line_buffer, "CURRENCY =", 10) == 0) {
            char* currency_name = trim_whitespace(line_buffer + 10);
            return currency_name;
        }

        free(line_buffer);
    }

    // Currency name was not found
    return NULL;
}

/**
 * @brief Parse a single coin denomination block and return a Coin structure.
 *
 * @param coin_block_data The data for the coin block.
 * @param currency_name The name of the currency.
 * @return The parsed Coin structure.
 */
Coin parse_coin_block(const char* coin_block_data, const char* currency_name) {
    Coin coin = {0};
    coin.denomination.rules.cipher = strdup(""); // Allocate an empty string as a placeholder.
    coin.denomination.name = strdup(currency_name); // Set currency name

    // Temporary storage for duration strings
    char legal_duration_str[MAX_LINE_LENGTH] = {0};
    char spend_duration_str[MAX_LINE_LENGTH] = {0};
    char withdraw_duration_str[MAX_LINE_LENGTH] = {0};

    // Tokenize the block into lines
    char dataCopy[MAX_BLOCK_SIZE]; // Define a statically sized array.
    strncpy(dataCopy, coin_block_data, MAX_BLOCK_SIZE); // Copy coin block data.
    dataCopy[MAX_BLOCK_SIZE - 1] = '\0'; // Ensure null termination.

    char* line;
    char* rest = dataCopy;

    while((line = strtok_r(rest, "\n", &rest))) {
        trim_whitespace(line); // Clean up the line

        // Parse key-value pairs
        char* key;
        char* value;
        char* subrest = line; // Use subrest for the inner parsing of the line.
        key = strtok_r(subrest, "=", &subrest);
        value = subrest;

        if (key && value) {
            key = trim_whitespace(key);
            value = trim_whitespace(value);

            if (strcmp(key, "rsa_keysize") == 0) {
                coin.denomination.rules.rsa_keysize = atoi(value);
            } else if (strcmp(key, "fee_deposit") == 0) {
                coin.denomination.rules.fees.deposit_fee.fee_satoshis = parse_number(value);
            } else if (strcmp(key, "fee_refund") == 0) {
                coin.denomination.rules.fees.refund_fee.fee_satoshis = parse_number(value);
            } else if (strcmp(key, "fee_withdraw") == 0) {
                coin.denomination.rules.fees.withdraw_fee.fee_satoshis = parse_number(value);
            } else if (strcmp(key, "fee_refresh") == 0) {
                coin.denomination.rules.fees.refresh_fee.fee_satoshis = parse_number(value);
            } else if (strcmp(key, "duration_legal") == 0) {
                strncpy(legal_duration_str, value, sizeof(legal_duration_str) - 1);
            } else if (strcmp(key, "duration_spend") == 0) {
                strncpy(spend_duration_str, value, sizeof(spend_duration_str) - 1);
            } else if (strcmp(key, "duration_withdraw") == 0) {
                strncpy(withdraw_duration_str, value, sizeof(withdraw_duration_str) - 1);
            } else if (strcmp(key, "value") == 0) {
                coin.denomination.amount = parse_number(value);
            } else if (strcmp(key, "cipher") == 0) {
                free(coin.denomination.rules.cipher); // Free the placeholder
                coin.denomination.rules.cipher = strdup(value);
            }
        }

    }

    // Convert duration strings to seconds and set in the structure
    coin.denomination.rules.durations.legal.time = time_to_seconds(legal_duration_str);
    coin.denomination.rules.durations.deposit.time = time_to_seconds(spend_duration_str);
    coin.denomination.rules.durations.withdraw.time = time_to_seconds(withdraw_duration_str);

    return coin;
}

/**
 * @brief Read the entire file content into a string.
 *
 * @param filename The path to the file.
 * @return A string containing the file content, or NULL on failure.
 */
static char* read_file_into_string(const char* filename) {
    FILE* file = fopen(filename, "rb");
    char* buffer = NULL;
    if (file) {
        // Go to the end of the file
        if (fseek(file, 0L, SEEK_END) == 0) {
            // Get the size of the file
            long bufsize = ftell(file);
            if (bufsize == -1) { /* Error */ }

            // Allocate our buffer to that size
            buffer = malloc(sizeof(char) * (bufsize + 1));

            // Go back to the start of the file
            if (fseek(file, 0L, SEEK_SET) != 0) { /* Error */ }

            // Read the entire file into memory
            size_t newLen = fread(buffer, sizeof(char), bufsize, file);
            if (newLen == 0) {
                free(buffer);
                buffer = NULL;
            } else {
                buffer[newLen++] = '\0'; // Just to be safe
            }
        }
        fclose(file);
    }
    return buffer;
}

/**
 * @brief Parse the wallet configuration from a file.
 *
 * @param config_path The path to the configuration file.
 * @return The parsed Wallet structure.
 */
Wallet parse_wallet_config(const char* config_path) {
    Wallet wallet;
    wallet.coins = malloc(sizeof(Coin) * MAX_SECTIONS); // Allocate space for coins
    wallet.num_coins = 0; // Start with zero coins
    if (!wallet.coins) {
        // Memory allocation failed
        fprintf(stderr, "Failed to allocate memory for coins.\n");
        return wallet;
    }

    char* file_content = read_file_into_string(config_path);
    if (!file_content) {
        // Handle error: could not read file
        fprintf(stderr, "Failed to read file content.\n");
        return wallet;
    }

    char* currency_name = parse_currency_name(file_content);
    if (!currency_name) {
        fprintf(stderr, "Currency name not found.\n");
        free(file_content);
        return wallet;
    }

    FILE* file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file\n");
        return (Wallet){0}; // Return an empty wallet struct on failure
    }

    // Parse the [coins...] block to get the coins
    char line[MAX_LINE_LENGTH];
    char current_block[MAX_LINE_LENGTH * 100] = {0}; // Buffer for current coin block
    int isCoinBlockActive = 0;
    int emptyLineCount = 0;

    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline character
        line[strcspn(line, "\n")] = 0;

        if (strncmp(line, "[coin_", 6) == 0) {
            // Start of a new coin block
            if (isCoinBlockActive && strlen(current_block) > 0) {
                // Parse the previous coin block if we're already inside a coin block
                wallet.coins[wallet.num_coins++] = parse_coin_block(current_block, currency_name); // Placeholder for currency_name
                memset(current_block, 0, sizeof(current_block)); // Reset block buffer
            }
            isCoinBlockActive = 1; // Mark that we're inside a coin block
            emptyLineCount = 0; // Reset empty line count
            strcat(current_block, line);
            strcat(current_block, "\n");
        } else if (strcmp(line, "") == 0 && isCoinBlockActive) {
            // Handle empty line within a coin block
            emptyLineCount++;
            if (emptyLineCount >= 2) {
                // Two consecutive empty lines indicate the end of coin blocks
                break; // Exit the loop to stop parsing
            }
        } else if (isCoinBlockActive) {
            // We're within a coin block and line is not empty
            emptyLineCount = 0; // Reset empty line count as we have content
            strcat(current_block, line);
            strcat(current_block, "\n");
        }
    }

    // Parse the last coin block if not already done
    if (isCoinBlockActive && strlen(current_block) > 0) {
        wallet.coins[wallet.num_coins++] = parse_coin_block(current_block, currency_name);
    }

    fclose(file);

    return wallet; // Return the populated wallet
}

/**
 * @brief Parse the JSON configuration for a wallet.
 *
 * @param filename The path to the JSON configuration file.
 * @return The parsed Wallet structure.
 */
Wallet parse_wallet_config_json(const char* filename) {
    char* data = read_file_into_string(filename);
    if (!data) {
        fprintf(stderr, "Could not read the JSON configuration file.\n");
        return (Wallet){0};  // Return an empty Wallet on failure
    }

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        fprintf(stderr, "Error in JSON before: [%s]\n", cJSON_GetErrorPtr());
        free(data);
        return (Wallet){0};
    }

    // Extract currency
    const char *currency_name = cJSON_GetObjectItemCaseSensitive(json, "currency")->valuestring;

    Wallet wallet;
    wallet.num_coins = 0;
    wallet.coins = malloc(sizeof(Coin) * MAX_SECTIONS);

    cJSON *denominations = cJSON_GetObjectItemCaseSensitive(json, "denominations");
    cJSON *denom;
    cJSON_ArrayForEach(denom, denominations) {
        if (wallet.num_coins >= MAX_SECTIONS) break;

        Coin coin = {0};
        coin.denomination.name = strdup(currency_name);

        // Parse various fields from each denomination
        cJSON *value = cJSON_GetObjectItemCaseSensitive(denom, "value");
        coin.denomination.amount = parse_number(value->valuestring);

        cJSON *fee_withdraw = cJSON_GetObjectItemCaseSensitive(denom, "fee_withdraw");
        coin.denomination.rules.fees.withdraw_fee.fee_satoshis = parse_number(fee_withdraw->valuestring);

        cJSON *fee_deposit = cJSON_GetObjectItemCaseSensitive(denom, "fee_deposit");
        coin.denomination.rules.fees.deposit_fee.fee_satoshis = parse_number(fee_deposit->valuestring);

        cJSON *fee_refresh = cJSON_GetObjectItemCaseSensitive(denom, "fee_refresh");
        coin.denomination.rules.fees.refresh_fee.fee_satoshis = parse_number(fee_refresh->valuestring);

        cJSON *fee_refund = cJSON_GetObjectItemCaseSensitive(denom, "fee_refund");
        coin.denomination.rules.fees.refund_fee.fee_satoshis = parse_number(fee_refund->valuestring);

        cJSON *cipher = cJSON_GetObjectItemCaseSensitive(denom, "cipher");
        coin.denomination.rules.cipher = strdup(cipher->valuestring);

        cJSON *denoms_from_denom = cJSON_GetObjectItemCaseSensitive(denom, "denoms");
        cJSON *denom_item_first = cJSON_GetArrayItem(denoms_from_denom, 0);
        cJSON *stamp_start = cJSON_GetObjectItemCaseSensitive(denom_item_first, "stamp_start");
        cJSON *stamp_start_t_s = cJSON_GetObjectItemCaseSensitive(stamp_start, "t_s");

        cJSON *stamp_expire_withdraw = cJSON_GetObjectItemCaseSensitive(denom_item_first, "stamp_expire_withdraw");
        cJSON *stamp_expire_withdraw_t_s = cJSON_GetObjectItemCaseSensitive(stamp_expire_withdraw, "t_s");
        coin.denomination.rules.durations.withdraw.time = stamp_expire_withdraw_t_s->valueint - stamp_start_t_s->valueint;

        cJSON *stamp_expire_deposit = cJSON_GetObjectItemCaseSensitive(denom_item_first, "stamp_expire_deposit");
        cJSON *stamp_expire_deposit_t_s = cJSON_GetObjectItemCaseSensitive(stamp_expire_deposit, "t_s");
        coin.denomination.rules.durations.deposit.time = stamp_expire_deposit_t_s->valueint - stamp_start_t_s->valueint;

        cJSON *stamp_expire_legal = cJSON_GetObjectItemCaseSensitive(denom_item_first, "stamp_expire_legal");
        cJSON *stamp_expire_legal_t_s = cJSON_GetObjectItemCaseSensitive(stamp_expire_legal, "t_s");
        coin.denomination.rules.durations.legal.time = stamp_expire_legal_t_s->valueint - stamp_start_t_s->valueint;

        wallet.coins[wallet.num_coins++] = coin;
    }

    cJSON_Delete(json);
    free(data);
    return wallet;
}
