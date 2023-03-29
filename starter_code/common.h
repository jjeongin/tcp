#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED
extern int verbose;


#define NONE    0x0
#define INFO    0x1 
#define WARNING 0x10
#define INFO    0x1 
#define DEBUG   0x100
#define ALL     0x111

#define WINDOW_SIZE 10 // window size as 10
#define MAX_WINDOW_SIZE 100000 // max window size

#define VLOG(level, ... ) \
    if(level & verbose) { \
        fprintf(stderr, ##__VA_ARGS__ );\
        fprintf(stderr, "\n");\
    }\

void error(char *msg);
#endif

