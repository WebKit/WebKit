// Stub for usage_exit() required by tools_common.c when built as a library
// rather than as part of a CLI tool (vpxenc/vpxdec).
#include <stdlib.h>
void usage_exit(void) { abort(); }
