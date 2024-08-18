//
// Created by Bohdan Potuzhnyi on 14.03.2024.
// generator.c
//

#include "fee.h"
#include "user.h"
#include "coin_selection.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

const char* StrategyNames[] = {
        "MAX_BILLS",
        "MIN_BILLS",
        "CLOSEST_TO_EXPIRE_MIN_BILLS",
        "CLOSEST_TO_EXPIRE_MAX_BILLS",
        "MAX_BILLS_TIME_TO_EXPIRE_WEIGHTED",
        "RANDOM",
        "EVEN_FROM_MIN_TO_MAX",
        "EVEN_FROM_MAX_TO_MIN",
        "GREEDY_MIN_TO_MAX"
};

/**
 * @brief Generate actions for a given user based on their type.
 *
 * @param user The user for whom actions are being generated.
 * @param actions Pointer to an array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 */
void generate_actions_for_user(User user, Action **actions, int *size) {

    const int daysIn3Years = 360 * 3;

    switch (user.type) {
        case STUDENT_STATIC:
            generate_actions_for_student_static(actions, size, daysIn3Years);
            break;
        case STUDENT:
            generate_actions_for_student(actions, size, daysIn3Years);
            break;
        case BUSINESS_OWNER:
            generate_actions_for_business_owner(actions, size, daysIn3Years);
            break;
        case RETIRED:
            generate_actions_for_retired(actions, size, daysIn3Years);
            break;
        case FAMILY:
            generate_actions_for_family(actions, size, daysIn3Years);
            break;
        case FREELANCER:
            generate_actions_for_freelancer(actions, size, daysIn3Years);
            break;
        case TEACHER:
            generate_actions_for_teacher(actions, size, daysIn3Years);
            break;
        case ARTIST:
            generate_actions_for_artist(actions, size, daysIn3Years);
            break;
        default:
            *size = 0;
            *actions = NULL;
            break;
    }
}

/**
 * @brief Generate actions for multiple users and save them to CSV files.
 *
 * @param base_dir The base directory where the CSV files will be saved.
 * @param num_users The number of users for whom actions will be generated.
 */
void generate_and_save_actions(const char *base_dir, int num_users) {
    for (int user_i = 0; user_i < num_users; user_i++) {
        User user;
        char user_name[50];
        snprintf(user_name, sizeof(user_name), "user%d", user_i);
        user.name = user_name;
        user.type = rand() % NUMBER_OF_USERS;
        user.actions = NULL;
        int num_actions = 0;
        generate_actions_for_user(user, &user.actions, &num_actions);

        for (int strategy_i = 0; strategy_i < NUMBER_OF_STRATEGIES; strategy_i++) {
            char filename[256];
            sprintf(filename, "%s/user_%d_strategy_%s.csv", base_dir, user_i, StrategyNames[strategy_i]);

            FILE *file = fopen(filename, "w");
            if (file == NULL) {
                perror("Failed to open file");
                continue;
            }

            // Writing user details and strategy at the top
            fprintf(file, "%s,%d,%d,%d\n", user.name, user.type, strategy_i, user_i);

            // Writing actions
            for (int action_index = 0; action_index < num_actions; action_index++) {
                Action action = user.actions[action_index];
                fprintf(file, "%d,%lld,%lld\n", action.operation, action.amount, action.time);
            }
            fclose(file);
        }
        free(user.actions);
    }
}