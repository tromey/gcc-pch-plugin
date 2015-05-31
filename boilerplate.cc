#include "gcc-plugin.h"

#ifdef __GNUC__
#pragma GCC visibility push(default)
#endif
int plugin_is_GPL_compatible;
#ifdef __GNUC__
#pragma GCC visibility pop
#endif
