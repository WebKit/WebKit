#include <openssl/base.h>
#include <openssl/pki/verify_error.h>

namespace bssl {

VerifyError::VerifyError(StatusCode code, ptrdiff_t offset,
                         std::string diagnostic)
    : offset_(offset), code_(code), diagnostic_(std::move(diagnostic)) {}

const std::string &VerifyError::DiagnosticString() const { return diagnostic_; }

ptrdiff_t VerifyError::Index() const { return offset_; }

VerifyError::StatusCode VerifyError::Code() const { return code_; }

}  // namespacee bssl
