#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <namespace.h>

namespace SFRAME_NAMESPACE {

// Error types to replace exceptions
enum class SFrameErrorType
{
  internal_error = 1,
  invalid_parameter_error,
  buffer_too_small_error,
  crypto_error,
  unsupported_ciphersuite_error,
  authentication_error,
  invalid_key_usage_error,
  unknown_key_id_error,
};

class SFrameError
{
public:
  SFrameError(SFrameErrorType type)
    : type_(type)
    , message_(nullptr)
  {
  }

  SFrameError(SFrameErrorType type, const char* message)
    : type_(type)
    , message_(message)
  {
  }

  SFrameError(SFrameErrorType type, const char* message, uint64_t key_id)
    : type_(type)
    , message_(message)
    , key_id_(key_id)
  {
  }

  SFrameError(const SFrameError& other) = default;
  SFrameError(SFrameError&& other) noexcept = default;
  SFrameError& operator=(SFrameError&& other) noexcept = default;

  SFrameErrorType type() const { return type_; }

  const char* message() const { return message_; }

  // Populated only when type() == SFrameErrorType::unknown_key_id_error.
  std::optional<uint64_t> key_id() const { return key_id_; }

private:
  SFrameErrorType type_;
  // Message storage is borrowed; callers must pass a string with static or
  // otherwise stable lifetime.
  const char* message_ = nullptr;
  std::optional<uint64_t> key_id_ = std::nullopt;
};

#ifdef __cpp_exceptions
void
throw_sframe_error(const SFrameError& error);
#endif

template<typename T>
class Result
{
  static_assert(!std::is_same_v<T, SFrameError>,
                "Result<SFrameError> is not supported");

public:
  typedef T element_type;

  Result(SFrameError error)
    : data_(std::move(error))
  {
  }

  Result(SFrameErrorType error)
    : data_(SFrameError(error))
  {
  }

  Result(T value)
    : data_(std::move(value))
  {
  }

  Result(const Result& other) = delete;
  Result& operator=(const Result& other) = delete;
  Result(Result&& other) noexcept = default;
  Result& operator=(Result&& other) noexcept = default;

  T value() { return std::move(std::get<T>(data_)); }

  SFrameError error()
  {
    if (std::holds_alternative<SFrameError>(data_)) {
      return std::move(std::get<SFrameError>(data_));
    }

    return SFrameError(SFrameErrorType::internal_error);
  }

  bool is_ok() const { return std::holds_alternative<T>(data_); }

  bool is_err() const { return std::holds_alternative<SFrameError>(data_); }

#ifdef __cpp_exceptions
  T unwrap()
  {
    if (std::holds_alternative<SFrameError>(data_)) {
      throw_sframe_error(std::get<SFrameError>(data_));
    }

    return std::move(std::get<T>(data_));
  }
#endif

private:
  std::variant<T, SFrameError> data_;
};

// Specialization for Result<void>
template<>
class Result<void>
{
public:
  typedef void element_type;

  static Result ok() { return Result<void>(); }

  Result(SFrameError error)
    : error_(std::move(error))
  {
  }

  Result(SFrameErrorType error)
    : error_(SFrameError(error))
  {
  }

  Result() = default;

  Result(const Result& other) = delete;
  Result& operator=(const Result& other) = delete;
  Result(Result&& other) noexcept = default;
  Result& operator=(Result&& other) noexcept = default;

  // void has no value to move
  void value() {}

  SFrameError error() { return std::move(error_).value(); }

  bool is_ok() const { return !error_.has_value(); }

  bool is_err() const { return error_.has_value(); }

#ifdef __cpp_exceptions
  void unwrap()
  {
    if (error_.has_value()) {
      throw_sframe_error(error_.value());
    }
  }
#endif

private:
  std::optional<SFrameError> error_;
};

} // namespace SFRAME_NAMESPACE

// Unwrap a Result<T> into `var`, propagating the error by early return.
// Use in functions that already return Result<U>.
// Usage: SFRAME_VALUE_OR_RETURN(val, some_result_expr);
#define SFRAME_VALUE_OR_RETURN(var, expr)                                      \
  auto _sframe_r_##var = (expr);                                               \
  if (_sframe_r_##var.is_err()) {                                              \
    return _sframe_r_##var.error();                                            \
  }                                                                            \
  auto var = _sframe_r_##var.value()

// Propagate a Result<void> error by early return, discarding the void value.
// Use in functions that already return Result<U>.
// Usage: SFRAME_VOID_OR_RETURN(some_void_result_expr);
#define SFRAME_VOID_OR_RETURN(expr)                                            \
  do {                                                                         \
    auto _sframe_vr = (expr);                                                  \
    if (_sframe_vr.is_err()) {                                                 \
      return _sframe_vr.error();                                               \
    }                                                                          \
  } while (0)
