#ifndef LOG_H
#define LOG_H
#include <stdio.h>

#define log_printf(fmt, ...) do { \
	char s[256]; \
	snprintf(s, 256, fmt, __VA_ARGS__); \
	log_write(s); \
} while (0)

void log_setup();
void log_write(char *s);
#endif
