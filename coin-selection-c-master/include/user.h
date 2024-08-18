//
// Created by Bohdan Potuzhnyi and Vlada Svirsh on 13.03.2024.
// user.h
//

#ifndef USER_H
#define USER_H

#include "common.h"
#include "fee.h"

typedef enum{
    STUDENT,
    STUDENT_STATIC,
    BUSINESS_OWNER,
    RETIRED,
    FAMILY,
    FREELANCER,
    TEACHER,
    ARTIST,
    NUMBER_OF_USERS
} Type;

typedef struct {
    char* name;
    Type type;
    Action* actions;
    Wallet wallet;
} User;

void generate_actions_for_student(Action **actions, int *size, int days);
void generate_actions_for_student_static(Action **actions, int *size, int days);
void generate_actions_for_business_owner(Action **actions, int *size, int days);
void generate_actions_for_retired(Action **actions, int *size, int days);
void generate_actions_for_family(Action **actions, int *size, int days);
void generate_actions_for_freelancer(Action **actions, int *size, int days);
void generate_actions_for_teacher(Action **actions, int *size, int days);
void generate_actions_for_artist(Action **actions, int *size, int days);

#endif // USER_H
