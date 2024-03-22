#ifndef LOG_H
#define LOG_H

#define LOG_ERROR 1
#define LOG_INFO  2
#define LOG_DEBUG 3

void disp(int loglevel, const char* format, ...);
void set_log_level(int loglevel);

#endif
