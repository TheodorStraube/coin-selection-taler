#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fee.h"
#include "parser.h"
#include "user.h"
#include "common.h"
#include "generator.h"
#include "simulation.h"
#include "../src/cjson/cJSON.h"

#define MAX_THREADS 8
#define USER_NAME_MAX_LENGTH 50
#define SEMAPHORE_NAME "/semaphore"

sem_t* semaphore;
int total_files = 0;
int processed_files = 0;
time_t start_time;

/**
 * @brief Update the progress of file processing.
 */
void update_progress() {
    double elapsed = difftime(time(NULL), start_time);
    double avg_time_per_file = elapsed / processed_files;
    int remaining_files = total_files - processed_files;
    double est_time_remaining = avg_time_per_file * remaining_files;

    //system("clear"); // Clear the terminal (use "cls" on Windows)
    printf("Processing progress: %d/%d files\n", processed_files, total_files);
    printf("Estimated time remaining: %.2f seconds\n", est_time_remaining);
}

/**
 * @brief Count the number of files in a directory.
 *
 * @param directory_path The path to the directory.
 * @return The number of files in the directory.
 */
int count_files_in_directory(const char* directory_path) {
    int file_count = 0;
    DIR* dir = opendir(directory_path);
    struct dirent* entry;
    if (dir == NULL) {
        perror("Failed to open directory to count files");
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_count++;
        }
    }
    closedir(dir);
    return file_count;
}

/**
 * @brief Clear all files in a directory.
 *
 * @param directory_path The path to the directory to be cleared.
 */
void clear_directory(const char *directory_path) {
    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        perror("Failed to open directory for clearing");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Check if it is a regular file
            char filepath[1024];
            sprintf(filepath, "%s/%s", directory_path, entry->d_name);
            if (remove(filepath) != 0) {
                perror("Failed to delete file");
            }
        }
    }

    closedir(dir);
}

/**
 * @brief Simulate actions from a file for a given wallet.
 *
 * @param filepath The path to the file containing actions.
 * @param denomination_wallet The wallet with coin denominations.
 */
void simulate_actions_from_file(const char *filepath, Wallet denomination_wallet) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // First, determine the number of lines to allocate memory accordingly
    char buffer[1024];
    int line_count = 0;
    while (fgets(buffer, sizeof(buffer), file)) {
        line_count++;
    }

    if (line_count <= 1) { // Only the header line or empty file
        fclose(file);
        return;
    }

    // Allocate memory for actions based on line count minus one for the user info line
    Action *actions = malloc(sizeof(Action) * (line_count - 1));
    if (!actions) {
        perror("Memory allocation failed");
        fclose(file);
        return;
    }

    rewind(file); // Reset file pointer to the beginning to read again

    // Read the first line to parse user information
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        free(actions);
        fclose(file);
        return;
    }

    User user;
    user.name = malloc(USER_NAME_MAX_LENGTH);
    int user_index, strategy;
    sscanf(buffer, "%[^,],%d,%d,%d", user.name, &user.type, &strategy, &user_index);

    // Read the actions from the rest of the file
    int num_actions = 0;
    while (fgets(buffer, sizeof(buffer), file) != NULL && num_actions < line_count - 1) {
        sscanf(buffer, "%d,%lld,%lld", &actions[num_actions].operation, &actions[num_actions].amount, &actions[num_actions].time);
        num_actions++;
    }

    fclose(file);

    user.actions = actions;
    simulate_user_actions(user_index, user, denomination_wallet, num_actions, strategy);
}

/**
 * @brief Thread function to simulate actions from a file.
 *
 * @param arg The arguments for the thread, including the file path and wallet.
 * @return NULL
 */
void* thread_function(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    simulate_actions_from_file(args->filepath, args->denomination_wallet);

    sem_post(semaphore);

    // Remove file after processing
    if (remove(args->filepath) != 0) {
        perror("Failed to delete processed file");
    }

    free(arg);
    __sync_fetch_and_add(&processed_files, 1);
    update_progress();
    return NULL;
}

/**
 * @brief Load and simulate actions from all files in a directory.
 *
 * @param base_dir The base directory containing the files.
 * @param denomination_wallet The wallet with coin denominations.
 */
void load_and_simulate_actions(const char *base_dir, Wallet denomination_wallet) {
    DIR *dir;
    struct dirent *entry;

    total_files = count_files_in_directory(base_dir);
    processed_files = 0;
    start_time = time(NULL);

    if ((dir = opendir(base_dir)) == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".csv")) {
            char filepath[1024];
            sprintf(filepath, "%s/%s", base_dir, entry->d_name);
            sem_wait(semaphore);
            pthread_t thread;

            ThreadArgs* args = malloc(sizeof(ThreadArgs));
            if (args == NULL) {
                perror("Failed to allocate memory for thread args");
                continue;
            }

            args->filepath = strdup(filepath);
            args->denomination_wallet = denomination_wallet;

            if (pthread_create(&thread, NULL, thread_function, args) != 0) {
                perror("Failed to create thread");
                free(args);
                sem_post(semaphore); // Release the semaphore if thread creation fails
            } else {
                pthread_detach(thread); // Detach the thread
            }
        }
    }

    closedir(dir);

    while (processed_files < total_files) {
        sleep(1);
    }
}

/**
 * @brief Check if a directory exists and create it if it does not. If it exists, clear its contents.
 *
 * @param dir_path The path to the directory.
 */
void check_and_prepare_directory(const char *dir_path) {
    struct stat st = {0};

    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0700) == -1) {
            perror("Failed to create directory");
            exit(EXIT_FAILURE);
        }
    } else {
        clear_directory(dir_path);
    }
}

int init_python(){
    PyStatus status;

    printf("init_python\n");

    PyConfig config;
    PyConfig_InitPythonConfig(&config);

    status = Py_InitializeFromConfig(&config);
    if (PyStatus_Exception(status)) {
        return 1;
    }
    PyConfig_Clear(&config);
    
    PyObject *pName, *pModule, *pFunc, *pValue, *kwargs;

    pName = PyUnicode_FromString("main");
    /* Error checking of pName left out */

    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, "process_call");
        if (pFunc && PyCallable_Check(pFunc)) {
            printf("found function");
            kwargs = PyDict_New();

            PyObject *value = PyLong_FromLong(13);
            PyDict_SetItemString(kwargs, "amount", value);
            pValue = PyObject_CallOneArg(pFunc, kwargs);
            Py_DECREF(kwargs);
            if (pValue != NULL) {
                printf("Result of call: %ld\n", PyLong_AsLong(pValue));
                Py_DECREF(pValue);
            }else {
                
            }
        }else {printf("func error");}
    }else {
        printf("init error");
        PyErr_PrintEx(1);
    }
    

    return 0;
}
int checkOrLoadPytho() {
    PyObject *pName, *pModule, *pFunc, *pValue, *kwargs;

    PyStatus status;

    PyConfig config;

    if(pName != NULL){
        printf("name not null");
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

    printf("hier\n");
    if (pModule != NULL) {
        pFunc = PyObject_GetAttrString(pModule, "process_call");
        if (!pFunc || !PyCallable_Check(pFunc)) {
            printf("Failed to load Python");
            return 0;
        }
        printf("reached\n");
        pValue = PyObject_CallOneArg(pFunc, kwargs);
        return 1;        
    }
    printf("ier\n");
    return 0;
}

int main(int argc, char *argv[]) {
    int num_users = 1;

    if (argc > 1) {
        num_users = atoi(argv[1]);
        if (num_users <= 0) {
            fprintf(stderr, "Number of users must be a positive integer. Using default: 100\n");
            num_users = 100;
        }
    }
    
    //init_python();
    //checkOrLoadPytho();
    //return 0;
    srand(21);


    check_and_prepare_directory("../simulation");
    check_and_prepare_directory("../simulation/users");
    check_and_prepare_directory("../simulation/results");

    clear_directory("../simulation/users");
    clear_directory("../simulation/results");

    sem_unlink(SEMAPHORE_NAME);

    semaphore = sem_open(SEMAPHORE_NAME, O_CREAT, 0644, MAX_THREADS);
    if (semaphore == SEM_FAILED) {
        perror("Failed to open semaphore");
        exit(EXIT_FAILURE);
    }


    //  METHOD 1: Parse with txt file
    //    const char* config_filename = "../data/taler.conf";
    //    Wallet denomination_wallet = parse_wallet_config(config_filename);

    //  METHOD 2: Parse with json file
    const char* config_path = "../data/keys.json";

    Wallet denomination_wallet = parse_wallet_config_json(config_path);

    if (denomination_wallet.num_coins > 0) {
        printf("Loaded %d coins from configuration.\n", denomination_wallet.num_coins);
        for (int i = 0; i < denomination_wallet.num_coins; i++) {
            printf("Coin %d: %s\n", i+1, denomination_wallet.coins[i].denomination.name);
            printf("Amount: %lld satoshis\n", denomination_wallet.coins[i].denomination.amount);
            printf("Withdraw Fee: %lld satoshis\n", denomination_wallet.coins[i].denomination.rules.fees.withdraw_fee.fee_satoshis);
            printf("Refresh Fee: %lld satoshis\n", denomination_wallet.coins[i].denomination.rules.fees.refresh_fee.fee_satoshis);
            printf("Deposit Fee: %lld satoshis\n", denomination_wallet.coins[i].denomination.rules.fees.deposit_fee.fee_satoshis);
        }
    } else {
        printf("No coins were loaded from the configuration file.\n");
    }
    //  END OF PARSING

    // NEW METHOD OF GENERATING ACTIONS
    generate_and_save_actions("../simulation/users", num_users);

    // Load and simulate actions from the generated files
    load_and_simulate_actions("../simulation/users", denomination_wallet);
    PyObject *pValue, *pAmount, *kwargs;

    // Close and unlink the semaphore
    sem_close(semaphore);
    sem_unlink(SEMAPHORE_NAME);

    return 0;
}
