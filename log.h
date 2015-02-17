#ifndef _LOGGER_
#define _LOGGER_

extern int verbose;

#define LOG(...) if(verbose){fprintf(stderr, __VA_ARGS__);}

#endif
