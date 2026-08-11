// Copyright 2014 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>
#include <limits.h>
#include <string.h>

#if defined(BORINGSSL_FIPS)
#include <unistd.h>
#endif

#include <openssl/chacha.h>
#include <openssl/ctrdrbg.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "../../bcm_support.h"
#include "../../mem_internal.h"
#include "../bcm_interface.h"
#include "../delocate.h"
#include "internal.h"

#if defined(BORINGSSL_FIPS)
#include "../entropy/internal.h"
#endif


using namespace bssl;

// # Entropy design
//
// Each thread gets its own, thread-local DRBG. These are `rand_thread_state`
// objects. In non-FIPS mode these are seeded from the operating system. It's
// assumed that the operating system always has an unfailing source of entropy
// which is accessed via |CRYPTO_sysrand|. (If the operating system entropy
// source fails, it's up to |CRYPTO_sysrand| to abort the process—we don't try
// to handle it.)
//
// If running in FIPS mode, a compliant entropy source must be used to seed the
// thread-local DRBGs instead. That is either the BoringCrypto jitter source or,
// on Android, a source provided by the OS which is accessed via a UNIX domain
// socket. When seeding from these sources, OS entropy is still used via the
// "additional data" input to the DRBG. But, from a compliance perspective,
// this input is irrelevent.
//
// Since the jitter source takes some time to run, there is a single jitter
// source per address space. It's used to seed a DRBG (`core_drbg`) which, in
// turn, seeds the thread-local DRBGs. Android also uses this core DRBG design
// in order to keep things more uniform across platforms.
//
// The core DRBG will be reseeded in accordance with NIST's reseeding
// requirements. However, since our DRBG configuration allows for 2**48
// generate calls before reseeding, it's unlikely that any process would
// observe a reseed of that DRBG.
//
// The thread-local DRBGs mean that we have to worry about the process fork()ing
// or the underlying VM getting cloned. Thus we try to mix entropy into each
// generation from the thread-local DRBGs. The hardware may provide a
// low-latency RNG. (Intel's rdrand instruction is the canonical example of
// this.) If available, that'll be used for this additional data. Otherwise
// we'll draw from the OS if it doesn't provide other fork-detection APIs.
//
// (We assume that the OS entropy is safe from fork()ing and VM duplication.
// This might be a bit of a leap of faith, esp on Windows, but there's nothing
// that we can do about it.)

#if defined(BORINGSSL_FIPS)
// This #if-block needs to be outside of the anonymous namespace:

struct core_drbg {
  CTR_DRBG_STATE drbg;
  // calls is the number of generate calls made on |drbg| since it was last
  // (re)seeded. This is bound by |kCoreReseedInterval|.
  uint64_t calls = 0;
};

DEFINE_BSS_GET(struct core_drbg *, g_core_drbg, = nullptr)
DEFINE_STATIC_MUTEX(g_core_drbg_lock)
#endif  // FIPS

namespace {

#if defined(BORINGSSL_FIPS)

// The number of times we'll generate from the core DRBG before reseeding.
// This is within the permitted bounds but so high that this effectively means
// that we'll never reseed it. See SP 800-90Ar1 table 3.
constexpr uint64_t kCoreReseedInterval = (UINT64_C(1) << 48) - 1;
constexpr size_t kCoreDRBGEntropySize = 48;

#if defined(OPENSSL_ANDROID)
// Android seeds from a system daemon which is responsible for getting
// entropy from a validated source.
void core_drbg_get_entropy(uint8_t entropy[kCoreDRBGEntropySize]) {
  uint8_t daemon_buffer[BSSL_ENTROPY_DAEMON_RESPONSE_LEN];
  size_t daemon_buffer_len = sizeof(daemon_buffer);
  if (!bssl_get_seed_from_daemon(daemon_buffer, &daemon_buffer_len)) {
    // If the daemon isn't running then fallback to system entropy.
    CRYPTO_sysrand(entropy, kCoreDRBGEntropySize);
    return;
  }
  SHA384(daemon_buffer, daemon_buffer_len, entropy);
}
#else
// Non-Android platforms use the jitter source.
void core_drbg_get_entropy(uint8_t entropy[kCoreDRBGEntropySize]) {
  // This runs the jitter source and takes some milliseconds.
  for (unsigned i = 0; i < 100; i++) {
    // The internal health checks of the jitter source may cause spurious
    // failures, therefore we retry a number of times because failure
    // to get entropy is fatal.
    if (entropy::GetSeed(entropy)) {
      return;
    }
  }
  fprintf(stderr, "Persistent jitter source failure.\n");
  BORINGSSL_FIPS_abort();
}
#endif

struct core_drbg *core_drbg_get_locked() {
  struct core_drbg **core = g_core_drbg_bss_get();
  if (*core != nullptr) {
    return *core;
  }

  uint8_t entropy[kCoreDRBGEntropySize];
  core_drbg_get_entropy(entropy);

  *core = New<core_drbg>();
  if (!*core) {
    BORINGSSL_FIPS_abort();
  }

  uint8_t nonce[CTR_DRBG_NONCE_LEN] = {0};
  static constexpr char kPersonalization[] = "BoringSSL";
  if (!CTR_DRBG_init(&(*core)->drbg, /*df=*/1, entropy, sizeof(entropy), nonce,
                     reinterpret_cast<const uint8_t *>(kPersonalization),
                     sizeof(kPersonalization))) {
    BORINGSSL_FIPS_abort();
  }

  return *core;
}

// Draw a seed for a thread-local DRBG from the core DRBG.
void core_drbg_draw_seed(uint8_t *out_seed, size_t len) {
  MutexWriteLock lock(g_core_drbg_lock_bss_get());

  struct core_drbg *const core = core_drbg_get_locked();
  if (core->calls > kCoreReseedInterval) {
    uint8_t reseed_entropy[kCoreDRBGEntropySize];
    core_drbg_get_entropy(reseed_entropy);
    if (!CTR_DRBG_reseed(&core->drbg, reseed_entropy,
                         /*additional_data=*/nullptr, 0)) {
      BORINGSSL_FIPS_abort();
    }
    core->calls = 0;
  }

  if (!CTR_DRBG_generate(&core->drbg, out_seed, len,
                         /*additional_data=*/nullptr, 0)) {
    BORINGSSL_FIPS_abort();
  }
  core->calls++;
}

#endif  // BORINGSSL_FIPS

// kReseedInterval is the number of generate calls made to a thread-local
// CTR-DRBG before reseeding.
static const unsigned kReseedInterval = 4096;

// rand_thread_state contains the per-thread state for the RNG.
struct rand_thread_state {
  CTR_DRBG_STATE drbg;
  uint64_t fork_generation;
  // calls is the number of generate calls made on |drbg| since it was last
  // (re)seeded. This is bound by |kReseedInterval|.
  unsigned calls;
  // fork_unsafe_buffering is non-zero iff, when |drbg| was last (re)seeded,
  // fork-unsafe buffering was enabled.
  int fork_unsafe_buffering;

#if defined(BORINGSSL_FIPS)
  // next and prev form a nullptr-terminated, double-linked list of all states
  // in a process.
  struct rand_thread_state *next, *prev;
  // clear_drbg_lock synchronizes between uses of |drbg| and
  // |rand_thread_state_clear_all| clearing it. This lock should be uncontended
  // in the common case, except on shutdown.
  Mutex clear_drbg_lock;
#endif
};

}  // namespace

#if defined(BORINGSSL_FIPS)
// thread_states_list is the head of a linked-list of all |rand_thread_state|
// objects in the process, one per thread. This is needed because FIPS requires
// the ability to zero them on demand (AS09.28). BoringSSL triggers this with a
// destructor function.
DEFINE_BSS_GET(struct rand_thread_state *, thread_states_list, = nullptr)
DEFINE_STATIC_MUTEX(thread_states_list_lock)

static void rand_thread_state_clear_all() __attribute__((destructor));
static void rand_thread_state_clear_all() {
  thread_states_list_lock_bss_get()->LockWrite();
  for (struct rand_thread_state *cur = *thread_states_list_bss_get();
       cur != nullptr; cur = cur->next) {
    cur->clear_drbg_lock.LockWrite();
    CTR_DRBG_clear(&cur->drbg);
  }

  g_core_drbg_lock_bss_get()->LockWrite();
  struct core_drbg **core = g_core_drbg_bss_get();
  if (*core) {
    CTR_DRBG_clear(&(*core)->drbg);
  }

  // The locks are deliberately left locked so that any threads that are still
  // running will hang if they try to call |BCM_rand_bytes|. It also ensures
  // |rand_thread_state_free| cannot free any thread state while we've taken the
  // lock.
}
#endif

// rand_thread_state_free frees a |rand_thread_state|. This is called when a
// thread exits.
static void rand_thread_state_free(void *state_in) {
  struct rand_thread_state *state =
      reinterpret_cast<rand_thread_state *>(state_in);

  if (state_in == nullptr) {
    return;
  }

#if defined(BORINGSSL_FIPS)
  thread_states_list_lock_bss_get()->LockWrite();

  if (state->prev != nullptr) {
    state->prev->next = state->next;
  } else if (*thread_states_list_bss_get() == state) {
    // |state->prev| may be nullptr either if it is the head of the list,
    // or if |state| is freed before it was added to the list at all.
    // Compare against the head of the list to distinguish these cases.
    *thread_states_list_bss_get() = state->next;
  }

  if (state->next != nullptr) {
    state->next->prev = state->prev;
  }

  thread_states_list_lock_bss_get()->UnlockWrite();

  CTR_DRBG_clear(&state->drbg);
#endif

  Delete(state);
}

#if defined(OPENSSL_X86_64) && !defined(OPENSSL_NO_ASM) && \
    !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)
// rdrand should only be called if either |have_rdrand| or |have_fast_rdrand|
// returned true.
static int rdrand(uint8_t *buf, const size_t len) {
  const size_t len_multiple8 = len & ~7;
  if (!CRYPTO_rdrand_multiple8_buf(buf, len_multiple8)) {
    return 0;
  }
  const size_t remainder = len - len_multiple8;

  if (remainder != 0) {
    assert(remainder < 8);

    uint8_t rand_buf[8];
    if (!CRYPTO_rdrand(rand_buf)) {
      return 0;
    }
    OPENSSL_memcpy(buf + len_multiple8, rand_buf, remainder);
  }

  return 1;
}

#else

static int rdrand(uint8_t *buf, size_t len) { return 0; }

#endif

bcm_status bssl::BCM_rand_bytes_hwrng(uint8_t *buf, const size_t len) {
  if (!have_rdrand()) {
    return bcm_status::failure;
  }
  if (rdrand(buf, len)) {
    return bcm_status::not_approved;
  }
  return bcm_status::failure;
}

#if defined(BORINGSSL_FIPS)

// rand_get_seed fills |seed| with entropy. Since, in FIPS mode, this entropy
// comes from the jitter source / system daemon, `additional_input` will also be
// filled with system entropy.
static void rand_get_seed(struct rand_thread_state *state,
                          uint8_t seed[CTR_DRBG_ENTROPY_LEN],
                          uint8_t additional_input[CTR_DRBG_SEED_LEN],
                          size_t *out_additional_input_len) {
  core_drbg_draw_seed(seed, CTR_DRBG_ENTROPY_LEN);
  CRYPTO_sysrand(additional_input, CTR_DRBG_SEED_LEN);
  *out_additional_input_len = CTR_DRBG_SEED_LEN;
}

#else

// rand_get_seed fills |seed| with system entropy in a non-FIPS build.
static void rand_get_seed(struct rand_thread_state *state,
                          uint8_t seed[CTR_DRBG_ENTROPY_LEN],
                          uint8_t additional_input[CTR_DRBG_SEED_LEN],
                          size_t *out_additional_input_len) {
  CRYPTO_sysrand(seed, CTR_DRBG_ENTROPY_LEN);
  *out_additional_input_len = 0;
}

#endif

bcm_infallible bssl::BCM_rand_bytes_with_additional_data(
    uint8_t *out, size_t out_len, const uint8_t user_additional_data[32]) {
  if (out_len == 0) {
    return bcm_infallible::approved;
  }

  const uint64_t fork_generation = CRYPTO_get_fork_generation();
  const int fork_unsafe_buffering = rand_fork_unsafe_buffering_enabled();

  // Additional data is mixed into every CTR-DRBG call to protect, as best we
  // can, against forks & VM clones. We do not over-read this information and
  // don't reseed with it so, from the point of view of FIPS, this doesn't
  // provide “prediction resistance”. But, in practice, it does.
  uint8_t additional_data[32];
  // Intel chips have fast RDRAND instructions while, in other cases, RDRAND can
  // be _slower_ than a system call.
  if (!have_fast_rdrand() ||
      !rdrand(additional_data, sizeof(additional_data))) {
    // Without a hardware RNG to save us from address-space duplication, the OS
    // entropy is used. This can be expensive (one read per |RAND_bytes| call)
    // and so is disabled when we have fork detection, or if the application has
    // promised not to fork.
    if (fork_generation != 0 || fork_unsafe_buffering) {
      OPENSSL_memset(additional_data, 0, sizeof(additional_data));
    } else {
      CRYPTO_sysrand(additional_data, sizeof(additional_data));
    }
  }

  for (size_t i = 0; i < sizeof(additional_data); i++) {
    additional_data[i] ^= user_additional_data[i];
  }

  struct rand_thread_state stack_state;
  struct rand_thread_state *state = reinterpret_cast<rand_thread_state *>(
      CRYPTO_get_thread_local(OPENSSL_THREAD_LOCAL_RAND));

  if (state == nullptr) {
    state = New<rand_thread_state>();
    if (state == nullptr ||
        !CRYPTO_set_thread_local(OPENSSL_THREAD_LOCAL_RAND, state,
                                 rand_thread_state_free)) {
      // If the system is out of memory, use an ephemeral state on the
      // stack.
      Delete(state);
      state = &stack_state;
    }

    uint8_t seed[CTR_DRBG_ENTROPY_LEN];
    uint8_t personalization[CTR_DRBG_SEED_LEN] = {0};
    size_t personalization_len = 0;
    rand_get_seed(state, seed, personalization, &personalization_len);

    if (!CTR_DRBG_init(&state->drbg, /*df=*/true, seed,
                       sizeof(seed) - CTR_DRBG_NONCE_LEN,
                       seed + sizeof(seed) - CTR_DRBG_NONCE_LEN,
                       personalization, personalization_len)) {
      abort();
    }
    state->calls = 0;
    state->fork_generation = fork_generation;
    state->fork_unsafe_buffering = fork_unsafe_buffering;

#if defined(BORINGSSL_FIPS)
    if (state != &stack_state) {
      MutexWriteLock lock(thread_states_list_lock_bss_get());
      struct rand_thread_state **states_list = thread_states_list_bss_get();
      state->next = *states_list;
      if (state->next != nullptr) {
        state->next->prev = state;
      }
      state->prev = nullptr;
      *states_list = state;
    }
#endif
  }

  if (state->calls >= kReseedInterval ||
      // If we've forked since |state| was last seeded, reseed.
      state->fork_generation != fork_generation ||
      // If |state| was seeded from a state with different fork-safety
      // preferences, reseed. Suppose |state| was fork-safe, then forked into
      // two children, but each of the children never fork and disable fork
      // safety. The children must reseed to avoid working from the same PRNG
      // state.
      state->fork_unsafe_buffering != fork_unsafe_buffering) {
    uint8_t seed[CTR_DRBG_ENTROPY_LEN];
    uint8_t reseed_additional_data[CTR_DRBG_SEED_LEN] = {0};
    size_t reseed_additional_data_len = 0;
    rand_get_seed(state, seed, reseed_additional_data,
                  &reseed_additional_data_len);
#if defined(BORINGSSL_FIPS)
    // Take a read lock around accesses to |state->drbg|. This is needed to
    // avoid returning bad entropy if we race with
    // |rand_thread_state_clear_all|.
    state->clear_drbg_lock.LockRead();
#endif
    if (!CTR_DRBG_reseed_ex(&state->drbg, seed, sizeof(seed),
                            reseed_additional_data,
                            reseed_additional_data_len)) {
      abort();
    }
    state->calls = 0;
    state->fork_generation = fork_generation;
    state->fork_unsafe_buffering = fork_unsafe_buffering;
  } else {
#if defined(BORINGSSL_FIPS)
    state->clear_drbg_lock.LockRead();
#endif
  }

  int first_call = 1;
  while (out_len > 0) {
    size_t todo = out_len;
    if (todo > CTR_DRBG_MAX_GENERATE_LENGTH) {
      todo = CTR_DRBG_MAX_GENERATE_LENGTH;
    }

    if (!CTR_DRBG_generate(&state->drbg, out, todo, additional_data,
                           first_call ? sizeof(additional_data) : 0)) {
      abort();
    }

    out += todo;
    out_len -= todo;
    // Though we only check before entering the loop, this cannot add enough to
    // overflow a |size_t|.
    state->calls++;
    first_call = 0;
  }

  if (state == &stack_state) {
    CTR_DRBG_clear(&state->drbg);
  }

#if defined(BORINGSSL_FIPS)
  state->clear_drbg_lock.UnlockRead();
#endif
  return bcm_infallible::approved;
}

bcm_infallible bssl::BCM_rand_bytes(uint8_t *out, size_t out_len) {
  static const uint8_t kZeroAdditionalData[32] = {0};
  BCM_rand_bytes_with_additional_data(out, out_len, kZeroAdditionalData);
  return bcm_infallible::approved;
}
