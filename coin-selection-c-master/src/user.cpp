//
// Created by Bohdan Potuzhnyi on 30.04.2024.
//

// user.c
#include "user.h"
#include <stdlib.h>
#include <math.h>


/**
 * @brief Generate a random integer between min and max, inclusive.
 *
 * @param min The minimum value.
 * @param max The maximum value.
 * @return A random integer between min and max.
 */
int rand_range(int min, int max) {
    return rand() % (max - min + 1) + min;
}


/**
 * @brief Generate a random long long integer between min and max, inclusive.
 *
 * @param min The minimum value.
 * @param max The maximum value.
 * @return A random long long integer between min and max.
 */
long long rand_range_long(long long min, long long max) {
    if (min > max) {
        return 0; // Add error handling as appropriate
    }

    // The range of the random numbers
    long long range = max - min + 1;
    long long buckets = RAND_MAX / range;
    long long limit = buckets * range;

    // Ensure fairness and uniform distribution
    long long rand_val;
    do {
        rand_val = rand();
    } while (rand_val >= limit);

    return min + (rand_val / buckets);
}

/**
 * @brief Generate a random long long integer following a normal distribution.
 *
 * @param mean The mean of the normal distribution.
 * @param stddev The standard deviation of the normal distribution.
 * @return A random long long integer following a normal distribution.
 */
long long generate_normal_ll(double mean, double stddev) {
    static double n2 = 0.0;
    static int n2_cached = 0;
    double result;

    if (n2_cached) {
        n2_cached = 0;
        result = n2 * stddev + mean;
    } else {
        double x, y, r;
        do {
            x = 2.0 * rand() / (double)RAND_MAX - 1;
            y = 2.0 * rand() / (double)RAND_MAX - 1;
            r = x * x + y * y;
        } while (r == 0.0 || r > 1.0);
        {
            double d = sqrt(-2.0 * log(r) / r);
            double n1 = x * d;
            n2 = y * d;
            n2_cached = 1;
            result = n1 * stddev + mean;
        }
    }

    // Ensure result is non-negative and convert to long long
    if (result < 0) result = 0;
    return (long long)result;
}

/**
 * @brief Generate actions for a student user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_student(Action **actions, int *size, int days) {
    long long currentTimestamp;
    const long long secondsPerDay = 3600 * 24;
    long long accountBalance = 0;

    *size = 0;

    // Allocate memory for the maximum number of actions
    // Assuming up to 5 actions per day plus 2 for the first and last day of each month
    *actions = (Action *)malloc(sizeof(Action) * days * 6);

    for (int day = 1; day <= days; day++) {
        int dayOfMonth = (day - 1) % 30 + 1;
        int numActionsToday = rand_range(1, 5);
        currentTimestamp = (day - 1) * secondsPerDay;

        // First day of the month actions
        if (dayOfMonth == 1) {
            (*actions)[(*size)++] = (Action){.amount = 1500000, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += 1500000;
        }

        // Last day of the month, withdraw remaining balance
        if (dayOfMonth == 30) {
            long long amount = generate_normal_ll(accountBalance * 0.95, accountBalance * 0.1);
            if(amount > accountBalance) amount = accountBalance;
            (*actions)[(*size)++] = (Action){.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance = accountBalance - amount;
        } else {
            // Random transactions for the day
            for (int action = 0; action < numActionsToday; action++) {
                // Set amount for the transaction, ensuring it does not drop the account balance below 0
                long long amount = generate_normal_ll(10000, 5000); // Random amount between 5k and 15k satoshis
                if (accountBalance - amount < 0) {
                    // Adjust amount to not drop below 0
                    amount = accountBalance;
                }

                // Perform a withdrawal operation
                (*actions)[(*size)++] = (Action){.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
                accountBalance -= amount;

                // 1% chance for a refund operation, which logically returns funds from a mistaken withdrawal
                if (rand_range(1, 100) == 1) {
                    (*actions)[(*size)++] = (Action){.amount = amount, .operation = REFUND_OP, .time = currentTimestamp};
                    accountBalance += amount; // Increase account balance by the refunded amount
                }

                // Ensure account balance never drops below zero after adjustments
                if (accountBalance < 0) accountBalance = 0;
            }
        }
    }
}

/**
 * @brief Generate static actions for a student user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_student_static(Action **actions, int *size, int days){
    long long currentTimestamp;
    const long long secondsPerDay = 3600 * 24;
    long long accountBalance = 0;

    *actions = static_cast<Action *>(malloc(sizeof(Action) * days * 10)); // Allocate enough space considering maximum possible actions per day
    *size = 0;

    for (int day = 1; day <= days; day++) {
        int dayOfWeek = day % 7; // 0 for Sunday, 6 for Saturday
        currentTimestamp = (day - 1) * secondsPerDay;

        // Monthly stipend on the first day of each month and transport cost
        if (day % 30 == 1) {
            // Monthly stipend
            accountBalance += 2000000;
            (*actions)[(*size)++] = (Action){.amount = 2000000, .operation = WITHDRAW_OP, .time = currentTimestamp};

            // Transport cost
            accountBalance -= 240000;
            (*actions)[(*size)++] = (Action){.amount = 240000, .operation = DEPOSIT_OP, .time = currentTimestamp};
        }

        if (day % 30 == 0){
            // Last day of the month, deposit 100% of the balance
            long long depositAmount = accountBalance;
            accountBalance -= depositAmount;
            (*actions)[(*size)++] = (Action){.amount = depositAmount, .operation = DEPOSIT_OP, .time = currentTimestamp};
        }

        // Weekday spending
        if (dayOfWeek >= 1 && dayOfWeek <= 5) { // Monday to Friday
            for (int i = 0; i < 5; i++) {
                long long amount = 10000;
                if (accountBalance - amount < 0) amount = accountBalance;
                accountBalance -= amount;
                (*actions)[(*size)++] = (Action){.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            }
        } else { // Weekend spending
            long long amount = dayOfWeek == 6 ? 60000 : 30000; // Saturday for bar, Sunday for food
            if (accountBalance - amount < 0) amount = accountBalance;
            accountBalance -= amount;
            (*actions)[(*size)++] = (Action){.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
        }

        // Additional random transactions with 70% probability
        if (rand() % 100 < 70) {
            int numTransactions = rand_range(1, 3);
            for (int i = 0; i < numTransactions; i++) {
                long long amount = generate_normal_ll(7500, 5000);
                if (accountBalance - amount < 0) amount = accountBalance;
                accountBalance -= amount;
                (*actions)[(*size)++] = (Action){.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            }
        }

        // Ensure account balance never drops below zero
        if (accountBalance < 0) accountBalance = 0;
    }

}

/**
 * @brief Generate actions for a business owner user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_business_owner(Action **actions, int *size, int days){
    long long currentTimestamp;
    const long long secondsPerDay = 3600 * 24;
    long long accountBalance = 0;

    *size = 0; // Start with 0 actions and increment as we add actions

    // Allocate memory for the maximum number of actions, potentially more due to the daily deposits
    *actions = (Action *)malloc(sizeof(Action) * days * 12); // Adjusted for additional deposit actions

    for (int day = 1; day <= days; day++) {
        int dayOfMonth = (day - 1) % 30 + 1;
        currentTimestamp = (day - 1) * secondsPerDay; // Calculate the timestamp for the start of the current day

        // Daily deposits, 1 to 3 times a day
        int dailyDeposits = rand_range(1, 3);
        for (int deposit = 0; deposit < dailyDeposits; deposit++) {
            long long depositAmount = rand_range_long(100000, 1500000); // Random deposit amount
            (*actions)[(*size)++] = (Action){.amount = depositAmount, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += depositAmount;
        }

        if (dayOfMonth == 30) { // Last day of the month, deposit 80% of the balance
            long long depositAmount = accountBalance * 80 / 100;
            (*actions)[(*size)++] = (Action){.amount = depositAmount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= depositAmount;
        } else {
            // Regular expenses and occasional large investments or expenses
            int numActionsToday = rand_range(3, 8); // Business might have more transactions
            for (int action = 0; action < numActionsToday; action++) {
                // Randomly decide if this is a regular expense or a large investment/expense
                if (rand_range(1, 10) > 8) { // 20% chance for a large transaction
                    long long amount = generate_normal_ll(300000, 100000); // Large amount for investments/expenses
                    if (accountBalance - amount < 0) {
                        amount = accountBalance; // Adjust to avoid negative balance
                    }
                    (*actions)[(*size)++] = (Action) {.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
                    accountBalance -= amount;
                } else {
                    // Regular business expenses
                    long long amount = generate_normal_ll(50000, 20000); // Smaller amount for regular expenses
                    if (accountBalance - amount < 0) {
                        amount = accountBalance;
                    }
                    (*actions)[(*size)++] = (Action) {.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
                    accountBalance -= amount;
                }
            }

            // 2% chance for receiving a large refund related to business operations
            if (rand_range(1, 50) == 1) {
                long long refundAmount = rand_range_long(100000, 300000); // Larger refund amount
                (*actions)[(*size)++] = (Action) {.amount = refundAmount, .operation = REFUND_OP, .time = currentTimestamp};
                accountBalance += refundAmount;
            }
        }

        if (accountBalance < 0) accountBalance = 0;
    }
}

/**
 * @brief Generate actions for a retired user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_retired(Action **actions, int *size, int days){
    long long currentTimestamp;
    const long long secondsPerDay = 3600 * 24;
    long long accountBalance = 0;

    *size = 0; // Start with 0 actions and increment as we add actions

    // Allocate memory for the maximum number of actions, considering the pattern of transactions for retirees
    *actions = (Action *)malloc(sizeof(Action) * days * 4); // Fewer actions per day expected

    for (int day = 1; day <= days; day++) {
        int dayOfMonth = (day - 1) % 30 + 1;
        currentTimestamp = (day - 1) * secondsPerDay; // Calculate the timestamp for the start of the current day

        // Monthly pension or retirement savings withdrawal at the start of the month
        if (dayOfMonth == 1) {
            long long pensionAmount = rand_range_long(500000, 1000000); // Fixed income amount
            (*actions)[(*size)++] = (Action){.amount = pensionAmount, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += pensionAmount;
        }

        // Regular expenses
        if (rand_range(1, 30) <= 25) { // Most days have some routine expense
            long long expenseAmount = generate_normal_ll(30000, 15000);// Regular expense amount
            if (accountBalance - expenseAmount < 0) {
                expenseAmount = accountBalance;
            }
            (*actions)[(*size)++] = (Action){.amount = expenseAmount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= expenseAmount;
        }

        // Occasional large expense
        if (rand_range(1, 30) == 30) { // Once a month, potentially a large expense
            long long largeExpenseAmount = generate_normal_ll(200000, 100000); // Large expense for healthcare, travel, etc.
            if (accountBalance - largeExpenseAmount < 0) {
                largeExpenseAmount = accountBalance;
            }
            (*actions)[(*size)++] = (Action){.amount = largeExpenseAmount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= largeExpenseAmount;
        }

        // Investment income (if applicable)
        if (rand_range(1, 90) == 45) { // Quarterly dividends or interest
            long long investmentIncome = rand_range_long(20000, 100000); // Investment income amount
            (*actions)[(*size)++] = (Action){.amount = investmentIncome, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += investmentIncome;
        }

        if (accountBalance < 0) accountBalance = 0;
    }

}

/**
 * @brief Generate actions for a family user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_family(Action **actions, int *size, int days){
    long long currentTimestamp;
    const long long secondsPerDay = 3600 * 24;
    long long accountBalance = 0;

    *size = 0; // Start with 0 actions and increment as we add actions

    // Allocate memory considering a mix of regular and irregular transactions
    *actions = (Action *)malloc(sizeof(Action) * days * 8);

    for (int day = 1; day <= days; day++) {
        int dayOfMonth = (day - 1) % 30 + 1;
        currentTimestamp = (day - 1) * secondsPerDay; // Calculate the timestamp for the start of the current day

        // Monthly regular deposit from employment or other steady sources
        if (dayOfMonth == 1) {
            long long regularIncome = rand_range_long(2000000, 3000000); // Regular monthly income
            (*actions)[(*size)++] = (Action){.amount = regularIncome, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += regularIncome;
        }

        // Regular daily expenses with occasional larger monthly bills
        int numActionsToday = rand_range(2, 4); // Number of transactions for today
        for (int action = 0; action < numActionsToday; action++) {
            long long amount;
            // Simulate a larger expense on the first day of the month (e.g., rent, mortgage)
            if (dayOfMonth == 1 && action == 0) {
                amount = generate_normal_ll(350000, 150000); // Larger expense at the start of the month
            } else {
                amount = generate_normal_ll(30000, 10000); // Regular daily expense
            }

            if (accountBalance - amount < 0) {
                amount = accountBalance; // Adjust to avoid negative balance
            }
            (*actions)[(*size)++] = (Action){.amount = amount, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= amount;
        }

        // Occasional large deposits (e.g., bonuses, gifts)
        if (rand_range(1, 180) == 90) { // Twice a year
            long long largeDeposit = rand_range_long(500000, 1000000);
            (*actions)[(*size)++] = (Action){.amount = largeDeposit, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += largeDeposit;
        }

        // Periodic large withdrawals for significant expenses
        if (rand_range(1, 360) == 180) { // Once a year (e.g., tuition fees, medical bills)
            long long largeExpense = generate_normal_ll(500000, 200000);
            if (accountBalance - largeExpense < 0) {
                largeExpense = accountBalance; // Adjust to avoid negative balance
            }
            (*actions)[(*size)++] = (Action){.amount = largeExpense, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= largeExpense;
        }

        if (accountBalance < 0) accountBalance = 0;
    }
}

/**
 * @brief Generate actions for a freelancer user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_freelancer(Action **actions, int *size, int days) {
    long long currentTimestamp;
    const long long secondsPerDay = 86400;
    long long accountBalance = 500000; // Start with some initial balance

    *actions = static_cast<Action *>(malloc(sizeof(Action) * days * 4)); // Less frequent transactions than a business owner
    *size = 0;

    for (int day = 1; day <= days; day++) {
        currentTimestamp = (day - 1) * secondsPerDay;

        // Random chance of receiving payment
        if (rand_range(1, 30) == 1) { // Receives payment roughly once a month
            long long paymentAmount = rand_range_long(100000, 500000); // Payment amount varies
            (*actions)[(*size)++] = (Action){.amount = paymentAmount, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += paymentAmount;
        }

        // Daily expenses, much lower than income events
        long long expenseAmount = generate_normal_ll(20000, 5000); // Smaller daily living expenses
        if (accountBalance - expenseAmount < 0) expenseAmount = accountBalance;
        (*actions)[(*size)++] = (Action){.amount = expenseAmount, .operation = DEPOSIT_OP, .time = currentTimestamp};
        accountBalance -= expenseAmount;

        if (accountBalance < 0) accountBalance = 0; // Ensure balance doesn't drop below zero
    }
}

/**
 * @brief Generate actions for a teacher user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_teacher(Action **actions, int *size, int days) {
    long long currentTimestamp;
    const long long secondsPerDay = 86400;
    long long accountBalance = 0;

    *actions = (Action *)malloc(sizeof(Action) * days * 4); // Allocate memory for actions
    *size = 0;

    for (int day = 1; day <= days; day++) {
        int dayOfMonth = (day - 1) % 30 + 1;
        currentTimestamp = (day - 1) * secondsPerDay;

        // Monthly salary on the first day of each month
        if (dayOfMonth == 1) {
            long long salaryAmount = 3000000; // Fixed monthly salary
            (*actions)[(*size)++] = (Action){.amount = salaryAmount, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += salaryAmount;
        }

        // Daily expenses
        long long dailyExpense = 20000; // Small daily expenses
        if (accountBalance - dailyExpense < 0) dailyExpense = accountBalance;
        (*actions)[(*size)++] = (Action){.amount = dailyExpense, .operation = DEPOSIT_OP, .time = currentTimestamp};
        accountBalance -= dailyExpense;

        // Occasional large expenses, once a month
        if (dayOfMonth == 15) { // Middle of the month
            long long largeExpense = 100000; // Large expense for supplies or professional development
            if (accountBalance - largeExpense < 0) largeExpense = accountBalance;
            (*actions)[(*size)++] = (Action){.amount = largeExpense, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= largeExpense;
        }

        // Ensure account balance never drops below zero
        if (accountBalance < 0) accountBalance = 0;
    }
}

/**
 * @brief Generate actions for an artist user type.
 *
 * @param actions Pointer to the array of actions to be generated.
 * @param size Pointer to the size of the actions array.
 * @param days The number of days to simulate actions for.
 */
void generate_actions_for_artist(Action **actions, int *size, int days) {
    long long currentTimestamp;
    const long long secondsPerDay = 86400;
    long long accountBalance = 0;

    *actions = (Action *)malloc(sizeof(Action) * days * 6); // Allocate memory for actions, considering irregular income and expenses
    *size = 0;

    for (int day = 1; day <= days; day++) {
        currentTimestamp = (day - 1) * secondsPerDay;

        // Irregular income from sales or commissions
        if (rand_range(1, 3) == 1) { // Roughly once a few days
            long long incomeAmount = rand_range_long(500000, 2000000); // Varied income amounts
            (*actions)[(*size)++] = (Action){.amount = incomeAmount, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += incomeAmount;
        }

        // Regular daily expenses for art supplies
        long long dailyExpense = generate_normal_ll(20000, 5000); // Regular daily expenses
        if (accountBalance - dailyExpense < 0) dailyExpense = accountBalance;
        (*actions)[(*size)++] = (Action){.amount = dailyExpense, .operation = DEPOSIT_OP, .time = currentTimestamp};
        accountBalance -= dailyExpense;

        // Occasional large expenses for exhibitions or materials
        if (rand_range(1, 90) == 1) { // Roughly once every three months
            long long largeExpense = generate_normal_ll(300000, 100000); // Large expense amount
            if (accountBalance - largeExpense < 0) largeExpense = accountBalance;
            (*actions)[(*size)++] = (Action){.amount = largeExpense, .operation = DEPOSIT_OP, .time = currentTimestamp};
            accountBalance -= largeExpense;
        }

        // Potential grants or awards
        if (rand_range(1, 60) == 1) { // Once a couple of months
            long long grantAmount = rand_range_long(500000, 1000000); // Grant or award amount
            (*actions)[(*size)++] = (Action){.amount = grantAmount, .operation = WITHDRAW_OP, .time = currentTimestamp};
            accountBalance += grantAmount;
        }

        // Ensure account balance never drops below zero
        if (accountBalance < 0) accountBalance = 0;
    }
}


