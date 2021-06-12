/* Copyright (c) 2021, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <openssl/base.h>

#include <functional>
#include <memory>
#include <vector>

#include <openssl/span.h>


namespace bssl {
namespace acvp {

// kMaxArgs is the maximum number of arguments (including the function name)
// that an ACVP request can contain.
constexpr size_t kMaxArgs = 8;
// kMaxNameLength is the maximum length of a function name in an ACVP request.
constexpr size_t kMaxNameLength = 30;

// RequestBuffer holds various buffers needed for parsing an ACVP request. It
// can be reused between requests.
class RequestBuffer {
 public:
  virtual ~RequestBuffer();

  static std::unique_ptr<RequestBuffer> New();
};

// ParseArgsFromFd returns a span of arguments, the first of which is the name
// of the requested function, from |fd|. The return values point into |buffer|
// and so must not be used after |buffer| has been freed or reused for a
// subsequent call. It returns an empty span on error, because std::optional
// is still too new.
Span<const Span<const uint8_t>> ParseArgsFromFd(int fd, RequestBuffer *buffer);

// WriteReplyToFd writes a reply to the given file descriptor.
bool WriteReplyToFd(int fd, const std::vector<Span<const uint8_t>> &spans);

// ReplyCallback is the type of a callback that writes a reply to an ACVP
// request.
typedef std::function<bool(const std::vector<Span<const uint8_t>> &)>
    ReplyCallback;

// Handler is the type of a function that handles a specific ACVP request. If
// successful it will call |write_reply| with the response arguments and return
// |write_reply|'s return value. Otherwise it will return false. The given args
// must not include the name at the beginning.
typedef bool (*Handler)(const Span<const uint8_t> args[],
                        ReplyCallback write_reply);

// FindHandler returns a |Handler| that can process the given arguments, or logs
// a reason and returns |nullptr| if none is found.
Handler FindHandler(Span<const Span<const uint8_t>> args);

}  // namespace acvp
}  // namespace bssl
