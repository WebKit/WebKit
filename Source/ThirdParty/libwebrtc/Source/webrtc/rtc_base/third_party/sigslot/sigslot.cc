// sigslot.h: Signal/Slot classes
//
// Written by Sarah Thompson (sarah@telergy.com) 2002.
//
// License: Public domain. You are free to use this code however you like, with
// the proviso that the author takes on no responsibility or liability for any
// use.

#include "rtc_base/third_party/sigslot/sigslot.h"

#ifdef _SIGSLOT_HAS_POSIX_THREADS
#include <pthread.h>

namespace sigslot {

pthread_mutex_t* multi_threaded_global::get_mutex() {
  static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
  return &g_mutex;
}

}  // namespace sigslot

#endif  // _SIGSLOT_HAS_POSIX_THREADS
