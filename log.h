#ifndef _LOGGER_
#define _LOGGER_
#include <unistd.h>
#include <stdio.h>

#define LOG(...) if(logger != NULL){fprintf(logger, __VA_ARGS__);}

#endif
