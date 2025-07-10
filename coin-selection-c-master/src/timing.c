#include <timing.h>

void start(Timer *self) {
    self->accum = 0;
    self->start_time = clock();
}

float read(Timer *self){ 
    return (float)(clock() - self->start_time + self->accum) / CLOCKS_PER_SEC;
}

void pause(Timer *self) {
    self->accum += (clock() - self->start_time);
    // self->start_time = (clock_t)-1; 
}
void resume(Timer *self) {
    self->start_time = clock(); 
}
