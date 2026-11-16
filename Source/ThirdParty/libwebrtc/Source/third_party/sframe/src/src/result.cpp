#include <sframe/sframe.h>

namespace SFRAME_NAMESPACE {

#ifdef __cpp_exceptions
unsupported_ciphersuite_error::unsupported_ciphersuite_error()
  : std::runtime_error("Unsupported ciphersuite")
{
}

authentication_error::authentication_error()
  : std::runtime_error("AEAD authentication failure")
{
}

void
throw_sframe_error(const SFrameError& error)
{
  switch (error.type()) {
    case SFrameErrorType::internal_error:
      throw std::runtime_error(error.message() ? error.message()
                                               : "SFrame internal error");
    case SFrameErrorType::buffer_too_small_error:
      throw buffer_too_small_error(error.message());
    case SFrameErrorType::invalid_parameter_error:
      throw invalid_parameter_error(error.message());
    case SFrameErrorType::crypto_error:
      throw crypto_error();
    case SFrameErrorType::unsupported_ciphersuite_error:
      throw unsupported_ciphersuite_error();
    case SFrameErrorType::authentication_error:
      throw authentication_error();
    case SFrameErrorType::invalid_key_usage_error:
      throw invalid_key_usage_error(error.message());
    case SFrameErrorType::unknown_key_id_error:
      throw unknown_key_id_error(error.message());
  }
}
#endif

} // namespace SFRAME_NAMESPACE
