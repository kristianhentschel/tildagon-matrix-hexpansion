#ifndef DEBUG_PRINTF_H
#define DEBUG_PRINTF_H
#ifndef DISABLE_DEBUG_PRINTF
#include <stdio.h>
// Wrap around printf to avoid waiting for a debugger connection which can lock up during I2C setup
void debug_printf(const char* fmt, ...) {
  va_list(args);
  if (DidDebuggerAttach()) {
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}
#else
// if it's disabled don't include stdio at all and define as a noop to save space.
#define debug_printf(fmt, ...) {}
#endif
#endif