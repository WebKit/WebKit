#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

#include <sframe/sframe.h>

using namespace SFRAME_NAMESPACE;

// Fixed key material so the decrypt path is exercised.
static const uint8_t kKeyData[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
};

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  if (size == 0) {
    return 0;
  }

  // Use the first byte to select a cipher suite, remaining bytes as ciphertext.
  auto suite = static_cast<CipherSuite>((data[0] % 8) + 1);
  auto ciphertext = input_bytes(data + 1, size - 1);

  auto ctx = Context(suite);

  // Add keys for several KeyIDs so header-parsed IDs have a chance of matching.
  auto key = input_bytes(kKeyData, sizeof(kKeyData));
  for (KeyID kid = 0; kid < 16; kid++) {
    ctx.add_key(kid, KeyUsage::unprotect, key);
  }

  auto plaintext_buf = std::vector<uint8_t>(size);
  auto pt_out = output_bytes(plaintext_buf);

  std::ignore = ctx.unprotect(pt_out, ciphertext, {});

  return 0;
}
