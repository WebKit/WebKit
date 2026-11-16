#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <sframe/map.h>
#include <sframe/result.h>
#include <sframe/span.h>
#include <sframe/vector.h>

#ifdef __cpp_exceptions
#include <stdexcept>
#endif

#include <namespace.h>

// These constants define the size of certain internal data structures if
// we are configured not to depend on dynamic allocations, i.e., if the NO_ALLOC
// flag is set.  If you are using an allocator, you can ignore them.
//
// Note that these constants must be the same at the time when the library is
// built as at the time when it is used.  If you are using a pre-built binary,
// you must make sure that these parameters have the same values as when the
// library was built.
#ifndef SFRAME_MAX_KEYS
#define SFRAME_MAX_KEYS 200
#endif

#ifndef SFRAME_EPOCH_BITS
#define SFRAME_EPOCH_BITS 4
#endif

namespace SFRAME_NAMESPACE {

#ifdef __cpp_exceptions
struct crypto_error : std::runtime_error
{
  crypto_error();
};

struct unsupported_ciphersuite_error : std::runtime_error
{
  unsupported_ciphersuite_error();
};

struct authentication_error : std::runtime_error
{
  authentication_error();
};

struct buffer_too_small_error : std::runtime_error
{
  using parent = std::runtime_error;
  using parent::parent;
};

struct invalid_parameter_error : std::runtime_error
{
  using parent = std::runtime_error;
  using parent::parent;
};

struct invalid_key_usage_error : std::runtime_error
{
  using parent = std::runtime_error;
  using parent::parent;
};

struct unknown_key_id_error : std::runtime_error
{
  using parent = std::runtime_error;
  using parent::parent;
};
#endif

enum class CipherSuite : uint16_t
{
  AES_128_CTR_HMAC_SHA256_80 = 1,
  AES_128_CTR_HMAC_SHA256_64 = 2,
  AES_128_CTR_HMAC_SHA256_32 = 3,
  AES_GCM_128_SHA256 = 4,
  AES_GCM_256_SHA512 = 5,
  AES_256_CTR_HMAC_SHA512_80 = 6,
  AES_256_CTR_HMAC_SHA512_64 = 7,
  AES_256_CTR_HMAC_SHA512_32 = 8,
};

using input_bytes = span<const uint8_t>;
using output_bytes = span<uint8_t>;

template<size_t N>
using owned_bytes = vector<uint8_t, N>;

using KeyID = uint64_t;
using Counter = uint64_t;
class Header;

enum struct KeyUsage
{
  protect,
  unprotect,
};

struct KeyRecord
{
  static Result<KeyRecord> from_base_key(CipherSuite suite,
                                         KeyID key_id,
                                         KeyUsage usage,
                                         input_bytes base_key);

  // 32-byte AES-256 key + 64-byte HMAC-SHA512 key
  static constexpr size_t max_key_size = 96;
  static constexpr size_t max_salt_size = 12;

  owned_bytes<max_key_size> key;
  owned_bytes<max_salt_size> salt;
  KeyUsage usage;
  Counter counter;
};

// Context applies the full SFrame transform.  It tracks a counter for each key
// to ensure nonce uniqueness, adds the SFrame header on protect, and
// reads/strips the SFrame header on unprotect.
class Context
{
public:
  Context(CipherSuite suite);
  virtual ~Context();

  Result<void> add_key(KeyID kid, KeyUsage usage, input_bytes key);
  void remove_key(KeyID kid);

  Result<output_bytes> protect(KeyID key_id,
                               output_bytes ciphertext,
                               input_bytes plaintext,
                               input_bytes metadata);
  Result<output_bytes> unprotect(output_bytes plaintext,
                                 input_bytes ciphertext,
                                 input_bytes metadata);

  static constexpr size_t max_overhead = 17 + 16;
  static constexpr size_t max_metadata_size = 512;

protected:
  CipherSuite suite;
  map<KeyID, KeyRecord, SFRAME_MAX_KEYS> keys;

  Result<void> require_key(KeyID key_id) const;

  Result<output_bytes> protect_inner(const Header& header,
                                     output_bytes ciphertext,
                                     input_bytes plaintext,
                                     input_bytes metadata);
  Result<output_bytes> unprotect_inner(const Header& header,
                                       output_bytes ciphertext,
                                       input_bytes plaintext,
                                       input_bytes metadata);
};

// MLSContext augments Context with logic for deriving keys from MLS.  Instead
// of adding individual keys, salts, and key IDs, the caller adds a secret for
// an epoch, and keys / salts / key IDs are derived as needed.
class MLSContext : protected Context
{
public:
  using EpochID = uint64_t;
  using SenderID = uint64_t;
  using ContextID = uint64_t;

  MLSContext(CipherSuite suite_in, size_t epoch_bits_in);

  Result<void> add_epoch(EpochID epoch_id, input_bytes sframe_epoch_secret);
  Result<void> add_epoch(EpochID epoch_id,
                         input_bytes sframe_epoch_secret,
                         size_t sender_bits);
  void remove_epoch(EpochID epoch_id);
  void purge_before(EpochID keeper);

  Result<output_bytes> protect(EpochID epoch_id,
                               SenderID sender_id,
                               output_bytes ciphertext,
                               input_bytes plaintext,
                               input_bytes metadata);
  Result<output_bytes> protect(EpochID epoch_id,
                               SenderID sender_id,
                               ContextID context_id,
                               output_bytes ciphertext,
                               input_bytes plaintext,
                               input_bytes metadata);

  Result<output_bytes> unprotect(output_bytes plaintext,
                                 input_bytes ciphertext,
                                 input_bytes metadata);

private:
  // NOLINTBEGIN(clang-analyzer-core.uninitialized.Assign)
  struct EpochKeys
  {
    static constexpr size_t max_secret_size = 64;

    EpochID full_epoch;
    owned_bytes<max_secret_size> sframe_epoch_secret;
    size_t sender_bits;
    size_t context_bits;
    uint64_t max_sender_id;
    uint64_t max_context_id;

    EpochKeys() = default;
    static Result<EpochKeys> create(EpochID full_epoch_in,
                                    input_bytes sframe_epoch_secret_in,
                                    size_t epoch_bits,
                                    size_t sender_bits_in);
    Result<owned_bytes<max_secret_size>> base_key(CipherSuite suite,
                                                  SenderID sender_id) const;
  };
  // NOLINTEND(clang-analyzer-core.uninitialized.Assign)

  void purge_epoch(EpochID epoch_id);

  Result<KeyID> form_key_id(EpochID epoch_id,
                            SenderID sender_id,
                            ContextID context_id) const;
  Result<void> ensure_key(KeyID key_id, KeyUsage usage);

  const size_t epoch_bits;
  const size_t epoch_mask;

  static constexpr size_t max_epochs = 1 << SFRAME_EPOCH_BITS;
  vector<std::optional<EpochKeys>, max_epochs> epoch_cache;
};

} // namespace SFRAME_NAMESPACE
