#include <time.h>
#include "timing.h"

void init(Timer *self) {
    self->accum = 0;
}

float read(Timer *self){ 
    if(self->start_time == (clock_t) - 1) {
        return (float)(clock() - self->start_time + self->accum) / CLOCKS_PER_SEC;
    }
    return (float)self->accum / CLOCKS_PER_SEC;
}

void pause(Timer *self) {
    self->accum += (clock() - self->start_time);
    self->start_time = (clock_t)-1; 
}
void start(Timer *self) {
    self->start_time = clock(); 
}
