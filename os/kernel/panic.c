#include "defs.h"
// #ifndef TIMEVAL
// #define TIMEVAL
// typedef struct{
//     uint64 sec;
//     uint64 usec; 
// }TimeVal;
// #endif

void loop() {
    for(;;);
}

void panic(char *s)
{
    error("panic: %s", s);
    shutdown();
}