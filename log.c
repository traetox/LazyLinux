#include "log.h"
#include <stdarg.h>

extern FILE* logger;

void LOG(const char* format, ...) {
	va_list arg;
	if(logger == NULL) {
		return;
	}
	va_start (arg, format);
	vfprintf (logger, format, arg);
	fflush(logger);
	va_end (arg);
}
