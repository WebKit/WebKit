/*
 *  Copyright 2008 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/openssladapter.h"

#if defined(WEBRTC_POSIX)
#include <unistd.h>
#endif

// Must be included first before openssl headers.
#include "rtc_base/win32.h"  // NOLINT

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/openssl.h"
#include "rtc_base/sslroots.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/thread.h"

#ifndef OPENSSL_IS_BORINGSSL

// TODO: Use a nicer abstraction for mutex.

#if defined(WEBRTC_WIN)
  #define MUTEX_TYPE HANDLE
#define MUTEX_SETUP(x) (x) = CreateMutex(nullptr, FALSE, nullptr)
#define MUTEX_CLEANUP(x) CloseHandle(x)
#define MUTEX_LOCK(x) WaitForSingleObject((x), INFINITE)
#define MUTEX_UNLOCK(x) ReleaseMutex(x)
#define THREAD_ID GetCurrentThreadId()
#elif defined(WEBRTC_POSIX)
  #define MUTEX_TYPE pthread_mutex_t
  #define MUTEX_SETUP(x) pthread_mutex_init(&(x), nullptr)
  #define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
  #define MUTEX_LOCK(x) pthread_mutex_lock(&(x))
  #define MUTEX_UNLOCK(x) pthread_mutex_unlock(&(x))
  #define THREAD_ID pthread_self()
#else
  #error You must define mutex operations appropriate for your platform!
#endif

struct CRYPTO_dynlock_value {
  MUTEX_TYPE mutex;
};

#endif  // #ifndef OPENSSL_IS_BORINGSSL

//////////////////////////////////////////////////////////////////////
// SocketBIO
//////////////////////////////////////////////////////////////////////

static int socket_write(BIO* h, const char* buf, int num);
static int socket_read(BIO* h, char* buf, int size);
static int socket_puts(BIO* h, const char* str);
static long socket_ctrl(BIO* h, int cmd, long arg1, void* arg2);
static int socket_new(BIO* h);
static int socket_free(BIO* data);

// TODO(davidben): This should be const once BoringSSL is assumed.
static BIO_METHOD methods_socket = {
    BIO_TYPE_BIO, "socket",   socket_write, socket_read, socket_puts, 0,
    socket_ctrl,  socket_new, socket_free,  nullptr,
};

static BIO_METHOD* BIO_s_socket2() { return(&methods_socket); }

static BIO* BIO_new_socket(rtc::AsyncSocket* socket) {
  BIO* ret = BIO_new(BIO_s_socket2());
  if (ret == nullptr) {
    return nullptr;
  }
  ret->ptr = socket;
  return ret;
}

static int socket_new(BIO* b) {
  b->shutdown = 0;
  b->init = 1;
  b->num = 0; // 1 means socket closed
  b->ptr = 0;
  return 1;
}

static int socket_free(BIO* b) {
  if (b == nullptr)
    return 0;
  return 1;
}

static int socket_read(BIO* b, char* out, int outl) {
  if (!out)
    return -1;
  rtc::AsyncSocket* socket = static_cast<rtc::AsyncSocket*>(b->ptr);
  BIO_clear_retry_flags(b);
  int result = socket->Recv(out, outl, nullptr);
  if (result > 0) {
    return result;
  } else if (result == 0) {
    b->num = 1;
  } else if (socket->IsBlocking()) {
    BIO_set_retry_read(b);
  }
  return -1;
}

static int socket_write(BIO* b, const char* in, int inl) {
  if (!in)
    return -1;
  rtc::AsyncSocket* socket = static_cast<rtc::AsyncSocket*>(b->ptr);
  BIO_clear_retry_flags(b);
  int result = socket->Send(in, inl);
  if (result > 0) {
    return result;
  } else if (socket->IsBlocking()) {
    BIO_set_retry_write(b);
  }
  return -1;
}

static int socket_puts(BIO* b, const char* str) {
  return socket_write(b, str, rtc::checked_cast<int>(strlen(str)));
}

static long socket_ctrl(BIO* b, int cmd, long num, void* ptr) {
  switch (cmd) {
  case BIO_CTRL_RESET:
    return 0;
  case BIO_CTRL_EOF:
    return b->num;
  case BIO_CTRL_WPENDING:
  case BIO_CTRL_PENDING:
    return 0;
  case BIO_CTRL_FLUSH:
    return 1;
  default:
    return 0;
  }
}

static void LogSslError() {
  // Walk down the error stack to find the SSL error.
  uint32_t error_code;
  const char* file;
  int line;
  do {
    error_code = ERR_get_error_line(&file, &line);
    if (ERR_GET_LIB(error_code) == ERR_LIB_SSL) {
      RTC_LOG(LS_ERROR) << "ERR_LIB_SSL: " << error_code << ", " << file << ":"
                        << line;
      break;
    }
  } while (error_code != 0);
}

/////////////////////////////////////////////////////////////////////////////
// OpenSSLAdapter
/////////////////////////////////////////////////////////////////////////////

namespace rtc {

#ifndef OPENSSL_IS_BORINGSSL

// This array will store all of the mutexes available to OpenSSL.
static MUTEX_TYPE* mutex_buf = nullptr;

static void locking_function(int mode, int n, const char * file, int line) {
  if (mode & CRYPTO_LOCK) {
    MUTEX_LOCK(mutex_buf[n]);
  } else {
    MUTEX_UNLOCK(mutex_buf[n]);
  }
}

static unsigned long id_function() {  // NOLINT
  // Use old-style C cast because THREAD_ID's type varies with the platform,
  // in some cases requiring static_cast, and in others requiring
  // reinterpret_cast.
  return (unsigned long)THREAD_ID; // NOLINT
}

static CRYPTO_dynlock_value* dyn_create_function(const char* file, int line) {
  CRYPTO_dynlock_value* value = new CRYPTO_dynlock_value;
  if (!value)
    return nullptr;
  MUTEX_SETUP(value->mutex);
  return value;
}

static void dyn_lock_function(int mode, CRYPTO_dynlock_value* l,
                              const char* file, int line) {
  if (mode & CRYPTO_LOCK) {
    MUTEX_LOCK(l->mutex);
  } else {
    MUTEX_UNLOCK(l->mutex);
  }
}

static void dyn_destroy_function(CRYPTO_dynlock_value* l,
                                 const char* file, int line) {
  MUTEX_CLEANUP(l->mutex);
  delete l;
}

#endif  // #ifndef OPENSSL_IS_BORINGSSL

VerificationCallback OpenSSLAdapter::custom_verify_callback_ = nullptr;

bool OpenSSLAdapter::InitializeSSL(VerificationCallback callback) {
  if (!InitializeSSLThread() || !SSL_library_init())
      return false;
#if !defined(ADDRESS_SANITIZER) || !defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
  // Loading the error strings crashes mac_asan.  Omit this debugging aid there.
  SSL_load_error_strings();
#endif
  ERR_load_BIO_strings();
  OpenSSL_add_all_algorithms();
  RAND_poll();
  custom_verify_callback_ = callback;
  return true;
}

bool OpenSSLAdapter::InitializeSSLThread() {
  // BoringSSL is doing the locking internally, so the callbacks are not used
  // in this case (and are no-ops anyways).
#ifndef OPENSSL_IS_BORINGSSL
  mutex_buf = new MUTEX_TYPE[CRYPTO_num_locks()];
  if (!mutex_buf)
    return false;
  for (int i = 0; i < CRYPTO_num_locks(); ++i)
    MUTEX_SETUP(mutex_buf[i]);

  // we need to cast our id_function to return an unsigned long -- pthread_t is
  // a pointer
  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);
  CRYPTO_set_dynlock_create_callback(dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
#endif  // #ifndef OPENSSL_IS_BORINGSSL
  return true;
}

bool OpenSSLAdapter::CleanupSSL() {
#ifndef OPENSSL_IS_BORINGSSL
  if (!mutex_buf)
    return false;
  CRYPTO_set_id_callback(nullptr);
  CRYPTO_set_locking_callback(nullptr);
  CRYPTO_set_dynlock_create_callback(nullptr);
  CRYPTO_set_dynlock_lock_callback(nullptr);
  CRYPTO_set_dynlock_destroy_callback(nullptr);
  for (int i = 0; i < CRYPTO_num_locks(); ++i)
    MUTEX_CLEANUP(mutex_buf[i]);
  delete [] mutex_buf;
  mutex_buf = nullptr;
#endif  // #ifndef OPENSSL_IS_BORINGSSL
  return true;
}

OpenSSLAdapter::OpenSSLAdapter(AsyncSocket* socket,
                               OpenSSLAdapterFactory* factory)
    : SSLAdapter(socket),
      factory_(factory),
      state_(SSL_NONE),
      role_(SSL_CLIENT),
      ssl_read_needs_write_(false),
      ssl_write_needs_read_(false),
      restartable_(false),
      ssl_(nullptr),
      ssl_ctx_(nullptr),
      ssl_mode_(SSL_MODE_TLS),
      ignore_bad_cert_(false),
      custom_verification_succeeded_(false) {
  // If a factory is used, take a reference on the factory's SSL_CTX.
  // Otherwise, we'll create our own later.
  // Either way, we'll release our reference via SSL_CTX_free() in Cleanup().
  if (factory_) {
    ssl_ctx_ = factory_->ssl_ctx();
    RTC_DCHECK(ssl_ctx_);
    // Note: if using OpenSSL, requires version 1.1.0 or later.
    SSL_CTX_up_ref(ssl_ctx_);
  }
}

OpenSSLAdapter::~OpenSSLAdapter() {
  Cleanup();
}

void OpenSSLAdapter::SetIgnoreBadCert(bool ignore) {
  ignore_bad_cert_ = ignore;
}

void OpenSSLAdapter::SetAlpnProtocols(const std::vector<std::string>& protos) {
  alpn_protocols_ = protos;
}

void OpenSSLAdapter::SetEllipticCurves(const std::vector<std::string>& curves) {
  elliptic_curves_ = curves;
}

void OpenSSLAdapter::SetMode(SSLMode mode) {
  RTC_DCHECK(!ssl_ctx_);
  RTC_DCHECK(state_ == SSL_NONE);
  ssl_mode_ = mode;
}

void OpenSSLAdapter::SetIdentity(SSLIdentity* identity) {
  RTC_DCHECK(!identity_);
  identity_.reset(static_cast<OpenSSLIdentity*>(identity));
}

void OpenSSLAdapter::SetRole(SSLRole role) {
  role_ = role;
}

AsyncSocket* OpenSSLAdapter::Accept(SocketAddress* paddr) {
  RTC_DCHECK(role_ == SSL_SERVER);
  AsyncSocket* socket = SSLAdapter::Accept(paddr);
  if (!socket) {
    return nullptr;
  }

  SSLAdapter* adapter = SSLAdapter::Create(socket);
  adapter->SetIdentity(identity_->GetReference());
  adapter->SetRole(rtc::SSL_SERVER);
  adapter->SetIgnoreBadCert(ignore_bad_cert_);
  adapter->StartSSL("", false);
  return adapter;
}

int OpenSSLAdapter::StartSSL(const char* hostname, bool restartable) {
  if (state_ != SSL_NONE)
    return -1;

  ssl_host_name_ = hostname;
  restartable_ = restartable;

  if (socket_->GetState() != Socket::CS_CONNECTED) {
    state_ = SSL_WAIT;
    return 0;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    Error("BeginSSL", err, false);
    return err;
  }

  return 0;
}

int OpenSSLAdapter::BeginSSL() {
  RTC_LOG(LS_INFO) << "OpenSSLAdapter::BeginSSL: " << ssl_host_name_;
  RTC_DCHECK(state_ == SSL_CONNECTING);

  int err = 0;
  BIO* bio = nullptr;

  // First set up the context. We should either have a factory, with its own
  // pre-existing context, or be running standalone, in which case we will
  // need to create one, and specify |false| to disable session caching.
  if (!factory_) {
    RTC_DCHECK(!ssl_ctx_);
    ssl_ctx_ = CreateContext(ssl_mode_, false);
  }
  if (!ssl_ctx_) {
    err = -1;
    goto ssl_error;
  }

  if (identity_ && !identity_->ConfigureIdentity(ssl_ctx_)) {
    SSL_CTX_free(ssl_ctx_);
    err = -1;
    goto ssl_error;
  }

  bio = BIO_new_socket(socket_);
  if (!bio) {
    err = -1;
    goto ssl_error;
  }

  ssl_ = SSL_new(ssl_ctx_);
  if (!ssl_) {
    err = -1;
    goto ssl_error;
  }

  SSL_set_app_data(ssl_, this);

  // SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER allows different buffers to be passed
  // into SSL_write when a record could only be partially transmitted (and thus
  // requires another call to SSL_write to finish transmission). This allows us
  // to copy the data into our own buffer when this occurs, since the original
  // buffer can't safely be accessed after control exits Send.
  // TODO(deadbeef): Do we want SSL_MODE_ENABLE_PARTIAL_WRITE? It doesn't
  // appear Send handles partial writes properly, though maybe we never notice
  // since we never send more than 16KB at once..
  SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE |
                     SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  // Enable SNI, if a hostname is supplied.
  if (!ssl_host_name_.empty()) {
    SSL_set_tlsext_host_name(ssl_, ssl_host_name_.c_str());

    // Enable session caching, if configured and a hostname is supplied.
    if (factory_) {
      SSL_SESSION* cached = factory_->LookupSession(ssl_host_name_);
      if (cached) {
        if (SSL_set_session(ssl_, cached) == 0) {
          RTC_LOG(LS_WARNING) << "Failed to apply SSL session from cache";
          err = -1;
          goto ssl_error;
        }

        RTC_LOG(LS_INFO) << "Attempting to resume SSL session to "
                         << ssl_host_name_;
      }
    }
  }

  // Set a couple common TLS extensions; even though we don't use them yet.
  SSL_enable_ocsp_stapling(ssl_);
  SSL_enable_signed_cert_timestamps(ssl_);

  if (!alpn_protocols_.empty()) {
    std::string tls_alpn_string = TransformAlpnProtocols(alpn_protocols_);
    if (!tls_alpn_string.empty()) {
      SSL_set_alpn_protos(
          ssl_, reinterpret_cast<const unsigned char*>(tls_alpn_string.data()),
          tls_alpn_string.size());
    }
  }

  if (!elliptic_curves_.empty()) {
    SSL_set1_curves_list(ssl_, rtc::join(elliptic_curves_, ':').c_str());
  }

  // Now that the initial config is done, transfer ownership of |bio| to the
  // SSL object. If ContinueSSL() fails, the bio will be freed in Cleanup().
  SSL_set_bio(ssl_, bio, bio);
  bio = nullptr;

  // Do the connect.
  err = ContinueSSL();
  if (err != 0)
    goto ssl_error;

  return err;

ssl_error:
  Cleanup();
  if (bio)
    BIO_free(bio);

  return err;
}

int OpenSSLAdapter::ContinueSSL() {
  RTC_DCHECK(state_ == SSL_CONNECTING);

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_TIMEOUT);

  int code = (role_ == SSL_CLIENT) ? SSL_connect(ssl_) : SSL_accept(ssl_);
  switch (SSL_get_error(ssl_, code)) {
  case SSL_ERROR_NONE:
    if (!SSLPostConnectionCheck(ssl_, ssl_host_name_.c_str())) {
      RTC_LOG(LS_ERROR) << "TLS post connection check failed";
      // make sure we close the socket
      Cleanup();
      // The connect failed so return -1 to shut down the socket
      return -1;
    }

    state_ = SSL_CONNECTED;
    AsyncSocketAdapter::OnConnectEvent(this);
#if 0  // TODO: worry about this
    // Don't let ourselves go away during the callbacks
    PRefPtr<OpenSSLAdapter> lock(this);
    RTC_LOG(LS_INFO) << " -- onStreamReadable";
    AsyncSocketAdapter::OnReadEvent(this);
    RTC_LOG(LS_INFO) << " -- onStreamWriteable";
    AsyncSocketAdapter::OnWriteEvent(this);
#endif
    break;

  case SSL_ERROR_WANT_READ:
    RTC_LOG(LS_VERBOSE) << " -- error want read";
    struct timeval timeout;
    if (DTLSv1_get_timeout(ssl_, &timeout)) {
      int delay = timeout.tv_sec * 1000 + timeout.tv_usec/1000;

      Thread::Current()->PostDelayed(RTC_FROM_HERE, delay, this, MSG_TIMEOUT,
                                     0);
    }
    break;

  case SSL_ERROR_WANT_WRITE:
    break;

  case SSL_ERROR_ZERO_RETURN:
  default:
    RTC_LOG(LS_WARNING) << "ContinueSSL -- error " << code;
    return (code != 0) ? code : -1;
  }

  return 0;
}

void OpenSSLAdapter::Error(const char* context, int err, bool signal) {
  RTC_LOG(LS_WARNING) << "OpenSSLAdapter::Error(" << context << ", " << err
                      << ")";
  state_ = SSL_ERROR;
  SetError(err);
  if (signal)
    AsyncSocketAdapter::OnCloseEvent(this, err);
}

void OpenSSLAdapter::Cleanup() {
  RTC_LOG(LS_INFO) << "OpenSSLAdapter::Cleanup";

  state_ = SSL_NONE;
  ssl_read_needs_write_ = false;
  ssl_write_needs_read_ = false;
  custom_verification_succeeded_ = false;
  pending_data_.Clear();

  if (ssl_) {
    SSL_free(ssl_);
    ssl_ = nullptr;
  }

  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = nullptr;
  }
  identity_.reset();

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_TIMEOUT);
}

int OpenSSLAdapter::DoSslWrite(const void* pv, size_t cb, int* error) {
  // If we have pending data (that was previously only partially written by
  // SSL_write), we shouldn't be attempting to write anything else.
  RTC_DCHECK(pending_data_.empty() || pv == pending_data_.data());
  RTC_DCHECK(error != nullptr);

  ssl_write_needs_read_ = false;
  int ret = SSL_write(ssl_, pv, checked_cast<int>(cb));
  *error = SSL_get_error(ssl_, ret);
  switch (*error) {
    case SSL_ERROR_NONE:
      // Success!
      return ret;
    case SSL_ERROR_WANT_READ:
      RTC_LOG(LS_INFO) << " -- error want read";
      ssl_write_needs_read_ = true;
      SetError(EWOULDBLOCK);
      break;
    case SSL_ERROR_WANT_WRITE:
      RTC_LOG(LS_INFO) << " -- error want write";
      SetError(EWOULDBLOCK);
      break;
    case SSL_ERROR_ZERO_RETURN:
      // RTC_LOG(LS_INFO) << " -- remote side closed";
      SetError(EWOULDBLOCK);
      // do we need to signal closure?
      break;
    case SSL_ERROR_SSL:
      LogSslError();
      Error("SSL_write", ret ? ret : -1, false);
      break;
    default:
      RTC_LOG(LS_WARNING) << "Unknown error from SSL_write: " << *error;
      Error("SSL_write", ret ? ret : -1, false);
      break;
  }

  return SOCKET_ERROR;
}

//
// AsyncSocket Implementation
//

int OpenSSLAdapter::Send(const void* pv, size_t cb) {
  // RTC_LOG(LS_INFO) << "OpenSSLAdapter::Send(" << cb << ")";

  switch (state_) {
  case SSL_NONE:
    return AsyncSocketAdapter::Send(pv, cb);

  case SSL_WAIT:
  case SSL_CONNECTING:
    SetError(ENOTCONN);
    return SOCKET_ERROR;

  case SSL_CONNECTED:
    break;

  case SSL_ERROR:
  default:
    return SOCKET_ERROR;
  }

  int ret;
  int error;

  if (!pending_data_.empty()) {
    ret = DoSslWrite(pending_data_.data(), pending_data_.size(), &error);
    if (ret != static_cast<int>(pending_data_.size())) {
      // We couldn't finish sending the pending data, so we definitely can't
      // send any more data. Return with an EWOULDBLOCK error.
      SetError(EWOULDBLOCK);
      return SOCKET_ERROR;
    }
    // We completed sending the data previously passed into SSL_write! Now
    // we're allowed to send more data.
    pending_data_.Clear();
  }

  // OpenSSL will return an error if we try to write zero bytes
  if (cb == 0)
    return 0;

  ret = DoSslWrite(pv, cb, &error);

  // If SSL_write fails with SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE, this
  // means the underlying socket is blocked on reading or (more typically)
  // writing. When this happens, OpenSSL requires that the next call to
  // SSL_write uses the same arguments (though, with
  // SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER, the actual buffer pointer may be
  // different).
  //
  // However, after Send exits, we will have lost access to data the user of
  // this class is trying to send, and there's no guarantee that the user of
  // this class will call Send with the same arguements when it fails. So, we
  // buffer the data ourselves. When we know the underlying socket is writable
  // again from OnWriteEvent (or if Send is called again before that happens),
  // we'll retry sending this buffered data.
  if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
    // Shouldn't be able to get to this point if we already have pending data.
    RTC_DCHECK(pending_data_.empty());
    RTC_LOG(LS_WARNING)
        << "SSL_write couldn't write to the underlying socket; buffering data.";
    pending_data_.SetData(static_cast<const uint8_t*>(pv), cb);
    // Since we're taking responsibility for sending this data, return its full
    // size. The user of this class can consider it sent.
    return cb;
  }

  return ret;
}

int OpenSSLAdapter::SendTo(const void* pv,
                           size_t cb,
                           const SocketAddress& addr) {
  if (socket_->GetState() == Socket::CS_CONNECTED &&
      addr == socket_->GetRemoteAddress()) {
    return Send(pv, cb);
  }

  SetError(ENOTCONN);

  return SOCKET_ERROR;
}

int OpenSSLAdapter::Recv(void* pv, size_t cb, int64_t* timestamp) {
  // RTC_LOG(LS_INFO) << "OpenSSLAdapter::Recv(" << cb << ")";
  switch (state_) {

  case SSL_NONE:
    return AsyncSocketAdapter::Recv(pv, cb, timestamp);

  case SSL_WAIT:
  case SSL_CONNECTING:
    SetError(ENOTCONN);
    return SOCKET_ERROR;

  case SSL_CONNECTED:
    break;

  case SSL_ERROR:
  default:
    return SOCKET_ERROR;
  }

  // Don't trust OpenSSL with zero byte reads
  if (cb == 0)
    return 0;

  ssl_read_needs_write_ = false;

  int code = SSL_read(ssl_, pv, checked_cast<int>(cb));
  int error = SSL_get_error(ssl_, code);
  switch (error) {
    case SSL_ERROR_NONE:
      // RTC_LOG(LS_INFO) << " -- success";
      return code;
    case SSL_ERROR_WANT_READ:
      // RTC_LOG(LS_INFO) << " -- error want read";
      SetError(EWOULDBLOCK);
      break;
    case SSL_ERROR_WANT_WRITE:
      // RTC_LOG(LS_INFO) << " -- error want write";
      ssl_read_needs_write_ = true;
      SetError(EWOULDBLOCK);
      break;
    case SSL_ERROR_ZERO_RETURN:
      // RTC_LOG(LS_INFO) << " -- remote side closed";
      SetError(EWOULDBLOCK);
      // do we need to signal closure?
      break;
    case SSL_ERROR_SSL:
      LogSslError();
      Error("SSL_read", (code ? code : -1), false);
      break;
    default:
      RTC_LOG(LS_WARNING) << "Unknown error from SSL_read: " << error;
      Error("SSL_read", (code ? code : -1), false);
      break;
  }

  return SOCKET_ERROR;
}

int OpenSSLAdapter::RecvFrom(void* pv,
                             size_t cb,
                             SocketAddress* paddr,
                             int64_t* timestamp) {
  if (socket_->GetState() == Socket::CS_CONNECTED) {
    int ret = Recv(pv, cb, timestamp);

    *paddr = GetRemoteAddress();

    return ret;
  }

  SetError(ENOTCONN);

  return SOCKET_ERROR;
}

int OpenSSLAdapter::Close() {
  Cleanup();
  state_ = restartable_ ? SSL_WAIT : SSL_NONE;
  return AsyncSocketAdapter::Close();
}

Socket::ConnState OpenSSLAdapter::GetState() const {
  //if (signal_close_)
  //  return CS_CONNECTED;
  ConnState state = socket_->GetState();
  if ((state == CS_CONNECTED)
      && ((state_ == SSL_WAIT) || (state_ == SSL_CONNECTING)))
    state = CS_CONNECTING;
  return state;
}

bool OpenSSLAdapter::IsResumedSession() {
  return (ssl_ && SSL_session_reused(ssl_) == 1);
}

void OpenSSLAdapter::OnMessage(Message* msg) {
  if (MSG_TIMEOUT == msg->message_id) {
    RTC_LOG(LS_INFO) << "DTLS timeout expired";
    DTLSv1_handle_timeout(ssl_);
    ContinueSSL();
  }
}

void OpenSSLAdapter::OnConnectEvent(AsyncSocket* socket) {
  RTC_LOG(LS_INFO) << "OpenSSLAdapter::OnConnectEvent";
  if (state_ != SSL_WAIT) {
    RTC_DCHECK(state_ == SSL_NONE);
    AsyncSocketAdapter::OnConnectEvent(socket);
    return;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    AsyncSocketAdapter::OnCloseEvent(socket, err);
  }
}

void OpenSSLAdapter::OnReadEvent(AsyncSocket* socket) {
  // RTC_LOG(LS_INFO) << "OpenSSLAdapter::OnReadEvent";

  if (state_ == SSL_NONE) {
    AsyncSocketAdapter::OnReadEvent(socket);
    return;
  }

  if (state_ == SSL_CONNECTING) {
    if (int err = ContinueSSL()) {
      Error("ContinueSSL", err);
    }
    return;
  }

  if (state_ != SSL_CONNECTED)
    return;

  // Don't let ourselves go away during the callbacks
  //PRefPtr<OpenSSLAdapter> lock(this); // TODO: fix this
  if (ssl_write_needs_read_)  {
    // RTC_LOG(LS_INFO) << " -- onStreamWriteable";
    AsyncSocketAdapter::OnWriteEvent(socket);
  }

  // RTC_LOG(LS_INFO) << " -- onStreamReadable";
  AsyncSocketAdapter::OnReadEvent(socket);
}

void OpenSSLAdapter::OnWriteEvent(AsyncSocket* socket) {
  // RTC_LOG(LS_INFO) << "OpenSSLAdapter::OnWriteEvent";

  if (state_ == SSL_NONE) {
    AsyncSocketAdapter::OnWriteEvent(socket);
    return;
  }

  if (state_ == SSL_CONNECTING) {
    if (int err = ContinueSSL()) {
      Error("ContinueSSL", err);
    }
    return;
  }

  if (state_ != SSL_CONNECTED)
    return;

  // Don't let ourselves go away during the callbacks
  //PRefPtr<OpenSSLAdapter> lock(this); // TODO: fix this

  if (ssl_read_needs_write_)  {
    // RTC_LOG(LS_INFO) << " -- onStreamReadable";
    AsyncSocketAdapter::OnReadEvent(socket);
  }

  // If a previous SSL_write failed due to the underlying socket being blocked,
  // this will attempt finishing the write operation.
  if (!pending_data_.empty()) {
    int error;
    if (DoSslWrite(pending_data_.data(), pending_data_.size(), &error) ==
        static_cast<int>(pending_data_.size())) {
      pending_data_.Clear();
    }
  }

  // RTC_LOG(LS_INFO) << " -- onStreamWriteable";
  AsyncSocketAdapter::OnWriteEvent(socket);
}

void OpenSSLAdapter::OnCloseEvent(AsyncSocket* socket, int err) {
  RTC_LOG(LS_INFO) << "OpenSSLAdapter::OnCloseEvent(" << err << ")";
  AsyncSocketAdapter::OnCloseEvent(socket, err);
}

bool OpenSSLAdapter::VerifyServerName(SSL* ssl, const char* host,
                                      bool ignore_bad_cert) {
  if (!host)
    return false;

  // Checking the return from SSL_get_peer_certificate here is not strictly
  // necessary.  With our setup, it is not possible for it to return
  // null.  However, it is good form to check the return.
  X509* certificate = SSL_get_peer_certificate(ssl);
  if (!certificate)
    return false;

  // Logging certificates is extremely verbose. So it is disabled by default.
#ifdef LOG_CERTIFICATES
  {
    RTC_LOG(LS_INFO) << "Certificate from server:";
    BIO* mem = BIO_new(BIO_s_mem());
    X509_print_ex(mem, certificate, XN_FLAG_SEP_CPLUS_SPC, X509_FLAG_NO_HEADER);
    BIO_write(mem, "\0", 1);
    char* buffer;
    BIO_get_mem_data(mem, &buffer);
    RTC_LOG(LS_INFO) << buffer;
    BIO_free(mem);

    char* cipher_description =
        SSL_CIPHER_description(SSL_get_current_cipher(ssl), nullptr, 128);
    RTC_LOG(LS_INFO) << "Cipher: " << cipher_description;
    OPENSSL_free(cipher_description);
  }
#endif

  bool ok = false;
  GENERAL_NAMES* names = reinterpret_cast<GENERAL_NAMES*>(
      X509_get_ext_d2i(certificate, NID_subject_alt_name, nullptr, nullptr));
  if (names) {
    for (size_t i = 0; i < sk_GENERAL_NAME_num(names); i++) {
      const GENERAL_NAME* name = sk_GENERAL_NAME_value(names, i);
      if (name->type != GEN_DNS)
        continue;
      std::string value(
          reinterpret_cast<const char*>(ASN1_STRING_data(name->d.dNSName)),
          ASN1_STRING_length(name->d.dNSName));
      // string_match takes NUL-terminated strings, so check for embedded NULs.
      if (value.find('\0') != std::string::npos)
        continue;
      if (string_match(host, value.c_str())) {
        ok = true;
        break;
      }
    }
    GENERAL_NAMES_free(names);
  }

  char data[256];
  X509_NAME* subject;
  if (!ok && ((subject = X509_get_subject_name(certificate)) != nullptr) &&
      (X509_NAME_get_text_by_NID(subject, NID_commonName, data, sizeof(data)) >
       0)) {
    data[sizeof(data)-1] = 0;
    if (_stricmp(data, host) == 0)
      ok = true;
  }

  X509_free(certificate);

  // This should only ever be turned on for debugging and development.
  if (!ok && ignore_bad_cert) {
    RTC_LOG(LS_WARNING) << "TLS certificate check FAILED.  "
                        << "Allowing connection anyway.";
    ok = true;
  }

  return ok;
}

bool OpenSSLAdapter::SSLPostConnectionCheck(SSL* ssl, const char* host) {
  bool ok = VerifyServerName(ssl, host, ignore_bad_cert_);

  if (ok) {
    ok = (SSL_get_verify_result(ssl) == X509_V_OK ||
          custom_verification_succeeded_);
  }

  if (!ok && ignore_bad_cert_) {
    RTC_LOG(LS_INFO) << "Other TLS post connection checks failed.";
    ok = true;
  }

  return ok;
}

#if !defined(NDEBUG)

// We only use this for tracing and so it is only needed in debug mode

void OpenSSLAdapter::SSLInfoCallback(const SSL* s, int where, int ret) {
  const char* str = "undefined";
  int w = where & ~SSL_ST_MASK;
  if (w & SSL_ST_CONNECT) {
    str = "SSL_connect";
  } else if (w & SSL_ST_ACCEPT) {
    str = "SSL_accept";
  }
  if (where & SSL_CB_LOOP) {
    RTC_LOG(LS_INFO) << str << ":" << SSL_state_string_long(s);
  } else if (where & SSL_CB_ALERT) {
    str = (where & SSL_CB_READ) ? "read" : "write";
    RTC_LOG(LS_INFO) << "SSL3 alert " << str << ":"
                     << SSL_alert_type_string_long(ret) << ":"
                     << SSL_alert_desc_string_long(ret);
  } else if (where & SSL_CB_EXIT) {
    if (ret == 0) {
      RTC_LOG(LS_INFO) << str << ":failed in " << SSL_state_string_long(s);
    } else if (ret < 0) {
      RTC_LOG(LS_INFO) << str << ":error in " << SSL_state_string_long(s);
    }
  }
}

#endif

int OpenSSLAdapter::SSLVerifyCallback(int ok, X509_STORE_CTX* store) {
#if !defined(NDEBUG)
  if (!ok) {
    char data[256];
    X509* cert = X509_STORE_CTX_get_current_cert(store);
    int depth = X509_STORE_CTX_get_error_depth(store);
    int err = X509_STORE_CTX_get_error(store);

    RTC_LOG(LS_INFO) << "Error with certificate at depth: " << depth;
    X509_NAME_oneline(X509_get_issuer_name(cert), data, sizeof(data));
    RTC_LOG(LS_INFO) << "  issuer  = " << data;
    X509_NAME_oneline(X509_get_subject_name(cert), data, sizeof(data));
    RTC_LOG(LS_INFO) << "  subject = " << data;
    RTC_LOG(LS_INFO) << "  err     = " << err << ":"
                     << X509_verify_cert_error_string(err);
  }
#endif

  // Get our stream pointer from the store
  SSL* ssl = reinterpret_cast<SSL*>(
                X509_STORE_CTX_get_ex_data(store,
                  SSL_get_ex_data_X509_STORE_CTX_idx()));

  OpenSSLAdapter* stream =
    reinterpret_cast<OpenSSLAdapter*>(SSL_get_app_data(ssl));

  if (!ok && custom_verify_callback_) {
    void* cert =
        reinterpret_cast<void*>(X509_STORE_CTX_get_current_cert(store));
    if (custom_verify_callback_(cert)) {
      stream->custom_verification_succeeded_ = true;
      RTC_LOG(LS_INFO) << "validated certificate using custom callback";
      ok = true;
    }
  }

  // Should only be used for debugging and development.
  if (!ok && stream->ignore_bad_cert_) {
    RTC_LOG(LS_WARNING) << "Ignoring cert error while verifying cert chain";
    ok = 1;
  }

  return ok;
}

int OpenSSLAdapter::NewSSLSessionCallback(SSL* ssl, SSL_SESSION* session) {
  OpenSSLAdapter* stream =
      reinterpret_cast<OpenSSLAdapter*>(SSL_get_app_data(ssl));
  RTC_DCHECK(stream->factory_);
  RTC_LOG(LS_INFO) << "Caching SSL session for " << stream->ssl_host_name_;
  stream->factory_->AddSession(stream->ssl_host_name_, session);
  return 1;  // We've taken ownership of the session; OpenSSL shouldn't free it.
}

bool OpenSSLAdapter::ConfigureTrustedRootCertificates(SSL_CTX* ctx) {
  // Add the root cert that we care about to the SSL context
  int count_of_added_certs = 0;
  for (size_t i = 0; i < arraysize(kSSLCertCertificateList); i++) {
    const unsigned char* cert_buffer = kSSLCertCertificateList[i];
    size_t cert_buffer_len = kSSLCertCertificateSizeList[i];
    X509* cert =
        d2i_X509(nullptr, &cert_buffer, checked_cast<long>(cert_buffer_len));
    if (cert) {
      int return_value = X509_STORE_add_cert(SSL_CTX_get_cert_store(ctx), cert);
      if (return_value == 0) {
        RTC_LOG(LS_WARNING) << "Unable to add certificate.";
      } else {
        count_of_added_certs++;
      }
      X509_free(cert);
    }
  }
  return count_of_added_certs > 0;
}

SSL_CTX* OpenSSLAdapter::CreateContext(SSLMode mode, bool enable_cache) {
  // Use (D)TLS 1.2.
  // Note: BoringSSL supports a range of versions by setting max/min version
  // (Default V1.0 to V1.2). However (D)TLSv1_2_client_method functions used
  // below in OpenSSL only support V1.2.
  SSL_CTX* ctx = nullptr;
#ifdef OPENSSL_IS_BORINGSSL
  ctx = SSL_CTX_new(mode == SSL_MODE_DTLS ? DTLS_method() : TLS_method());
#else
  ctx = SSL_CTX_new(mode == SSL_MODE_DTLS ? DTLSv1_2_client_method()
                                          : TLSv1_2_client_method());
#endif  // OPENSSL_IS_BORINGSSL
  if (ctx == nullptr) {
    unsigned long error = ERR_get_error();  // NOLINT: type used by OpenSSL.
    RTC_LOG(LS_WARNING) << "SSL_CTX creation failed: " << '"'
                        << ERR_reason_error_string(error) << "\" "
                        << "(error=" << error << ')';
    return nullptr;
  }
  if (!ConfigureTrustedRootCertificates(ctx)) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

#if !defined(NDEBUG)
  SSL_CTX_set_info_callback(ctx, SSLInfoCallback);
#endif

  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, SSLVerifyCallback);
  SSL_CTX_set_verify_depth(ctx, 4);
  // Use defaults, but disable HMAC-SHA256 and HMAC-SHA384 ciphers
  // (note that SHA256 and SHA384 only select legacy CBC ciphers).
  // Additionally disable HMAC-SHA1 ciphers in ECDSA. These are the remaining
  // CBC-mode ECDSA ciphers.
  SSL_CTX_set_cipher_list(
      ctx, "ALL:!SHA256:!SHA384:!aPSK:!ECDSA+SHA1:!ADH:!LOW:!EXP:!MD5");

  if (mode == SSL_MODE_DTLS) {
    SSL_CTX_set_read_ahead(ctx, 1);
  }

  if (enable_cache) {
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT);
    SSL_CTX_sess_set_new_cb(ctx, &OpenSSLAdapter::NewSSLSessionCallback);
  }

  return ctx;
}

std::string TransformAlpnProtocols(
    const std::vector<std::string>& alpn_protocols) {
  // Transforms the alpn_protocols list to the format expected by
  // Open/BoringSSL. This requires joining the protocols into a single string
  // and prepending a character with the size of the protocol string before
  // each protocol.
  std::string transformed_alpn;
  for (const std::string& proto : alpn_protocols) {
    if (proto.size() == 0 || proto.size() > 0xFF) {
      RTC_LOG(LS_ERROR) << "OpenSSLAdapter::Error("
                        << "TransformAlpnProtocols received proto with size "
                        << proto.size() << ")";
      return "";
    }
    transformed_alpn += static_cast<char>(proto.size());
    transformed_alpn += proto;
    RTC_LOG(LS_VERBOSE) << "TransformAlpnProtocols: Adding proto: " << proto;
  }
  return transformed_alpn;
}

//////////////////////////////////////////////////////////////////////
// OpenSSLAdapterFactory
//////////////////////////////////////////////////////////////////////

OpenSSLAdapterFactory::OpenSSLAdapterFactory()
    : ssl_mode_(SSL_MODE_TLS), ssl_ctx_(nullptr) {}

OpenSSLAdapterFactory::~OpenSSLAdapterFactory() {
  for (auto it : sessions_) {
    SSL_SESSION_free(it.second);
  }
  SSL_CTX_free(ssl_ctx_);
}

void OpenSSLAdapterFactory::SetMode(SSLMode mode) {
  RTC_DCHECK(!ssl_ctx_);
  ssl_mode_ = mode;
}

OpenSSLAdapter* OpenSSLAdapterFactory::CreateAdapter(AsyncSocket* socket) {
  if (!ssl_ctx_) {
    bool enable_cache = true;
    ssl_ctx_ = OpenSSLAdapter::CreateContext(ssl_mode_, enable_cache);
    if (!ssl_ctx_) {
      return nullptr;
    }
  }

  return new OpenSSLAdapter(socket, this);
}

SSL_SESSION* OpenSSLAdapterFactory::LookupSession(const std::string& hostname) {
  auto it = sessions_.find(hostname);
  return (it != sessions_.end()) ? it->second : nullptr;
}

void OpenSSLAdapterFactory::AddSession(const std::string& hostname,
                                       SSL_SESSION* new_session) {
  SSL_SESSION* old_session = LookupSession(hostname);
  SSL_SESSION_free(old_session);
  sessions_[hostname] = new_session;
}

} // namespace rtc
