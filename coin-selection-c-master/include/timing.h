#include <time.h>

typedef struct Timer {
    clock_t start_time;
    long long accum;

    void (*start)();
    void (*pause)();
    void (*resume)();
    float (*read)();

} Timer;
