#include <time.h>

typedef struct Timer {
    clock_t start_time;
    long long accum;

    void (*start)();
    void (*pause_timer)();
    void (*init)();
    float (*read_timer)();

} Timer;
