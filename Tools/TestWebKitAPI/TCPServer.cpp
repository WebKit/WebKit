/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TCPServer.h"

#include <netinet/in.h>
#include <thread>
#include <unistd.h>
#include <wtf/Optional.h>
#include <wtf/text/Base64.h>

#if HAVE(SSL)

#define STACK_OF(type) struct stack_st_##type

extern "C" {

enum ssl_verify_result_t {
    ssl_verify_ok,
    ssl_verify_invalid,
    ssl_verify_retry,
};

struct BIO;
struct CRYPTO_BUFFER;
struct SSL_CTX;
struct EVP_PKEY;
struct SSL_METHOD;
struct SSL_PRIVATE_KEY_METHOD;
struct _STACK;
struct CRYPTO_BUFFER_POOL;
struct pem_password_cb;
int BIO_free(BIO*);
int SSL_free(SSL*);
int SSL_CTX_free(SSL_CTX*);
int EVP_PKEY_free(EVP_PKEY*);
int SSL_library_init();
const SSL_METHOD* TLS_with_buffers_method();
BIO* BIO_new_mem_buf(const void*, int);
EVP_PKEY* PEM_read_bio_PrivateKey(BIO*, EVP_PKEY**, pem_password_cb*, void*);
SSL_CTX* SSL_CTX_new(const SSL_METHOD*);
SSL* SSL_new(SSL_CTX*);
int SSL_accept(SSL*);
int SSL_set_fd(SSL*, int);
int SSL_get_error(const SSL*, int);
void SSL_CTX_set_custom_verify(SSL_CTX*, int mode, enum ssl_verify_result_t (*callback)(SSL *ssl, uint8_t *out_alert));
int SSL_read(SSL*, void*, int);
int SSL_write(SSL*, const void*, int);
const uint8_t* CRYPTO_BUFFER_data(const CRYPTO_BUFFER*);
size_t CRYPTO_BUFFER_len(const CRYPTO_BUFFER*);
void OPENSSL_free(void*);
int SSL_CTX_set_chain_and_key(SSL_CTX*, CRYPTO_BUFFER *const *certs, size_t num_certs, EVP_PKEY*, const SSL_PRIVATE_KEY_METHOD*);
CRYPTO_BUFFER* CRYPTO_BUFFER_new(const uint8_t*, size_t, CRYPTO_BUFFER_POOL*);
void CRYPTO_BUFFER_free(CRYPTO_BUFFER*);
size_t sk_num(const _STACK*);
void* sk_value(const _STACK*, size_t);
const STACK_OF(CRYPTO_BUFFER) *SSL_get0_peer_certificates(const SSL*);
void SSL_CTX_set_max_proto_version(SSL_CTX*, uint16_t);
#define SSL_VERIFY_PEER 0x01
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02

} // extern "C"

inline size_t sk_CRYPTO_BUFFER_num(const STACK_OF(CRYPTO_BUFFER) *sk) { return sk_num((const _STACK *)sk); }
inline CRYPTO_BUFFER* sk_CRYPTO_BUFFER_value(const STACK_OF(CRYPTO_BUFFER) *sk, size_t i) { return (CRYPTO_BUFFER *)sk_value((const _STACK *)sk, i); }
#endif // HAVE(SSL)

namespace TestWebKitAPI {

#if HAVE(SSL)
template<typename> struct deleter;
template<> struct deleter<BIO> {
    void operator()(BIO* bio)
    {
        BIO_free(bio);
    }
};
template<> struct deleter<SSL> {
    void operator()(SSL* ssl)
    {
        SSL_free(ssl);
    }
};
template<> struct deleter<SSL_CTX> {
    void operator()(SSL_CTX* ctx)
    {
        SSL_CTX_free(ctx);
    }
};
template<> struct deleter<EVP_PKEY> {
    void operator()(EVP_PKEY* key)
    {
        EVP_PKEY_free(key);
    }
};
template<> struct deleter<CRYPTO_BUFFER> {
    void operator()(CRYPTO_BUFFER* buffer)
    {
        CRYPTO_BUFFER_free(buffer);
    }
};
namespace ssl {
template <typename T> using unique_ptr = std::unique_ptr<T, deleter<T>>;
}
#endif // HAVE(SSL)

TCPServer::TCPServer(Function<void(Socket)>&& connectionHandler, size_t connections)
    : m_connectionHandler(WTFMove(connectionHandler))
{
    listenForConnections(connections);
}

#if HAVE(SSL)
TCPServer::TCPServer(Protocol protocol, Function<void(SSL*)>&& secureConnectionHandler, Optional<uint16_t> maxTLSVersion)
{
    auto startSecureConnection = [secureConnectionHandler = WTFMove(secureConnectionHandler), protocol, maxTLSVersion] (Socket socket) {
        SSL_library_init();

        ssl::unique_ptr<SSL_CTX> ctx(SSL_CTX_new(TLS_with_buffers_method()));

        // This is a test certificate from BoringSSL.
        String certPEM(
        "MIICWDCCAcGgAwIBAgIJAPuwTC6rEJsMMA0GCSqGSIb3DQEBBQUAMEUxCzAJBgNV"
        "BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX"
        "aWRnaXRzIFB0eSBMdGQwHhcNMTQwNDIzMjA1MDQwWhcNMTcwNDIyMjA1MDQwWjBF"
        "MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50"
        "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB"
        "gQDYK8imMuRi/03z0K1Zi0WnvfFHvwlYeyK9Na6XJYaUoIDAtB92kWdGMdAQhLci"
        "HnAjkXLI6W15OoV3gA/ElRZ1xUpxTMhjP6PyY5wqT5r6y8FxbiiFKKAnHmUcrgfV"
        "W28tQ+0rkLGMryRtrukXOgXBv7gcrmU7G1jC2a7WqmeI8QIDAQABo1AwTjAdBgNV"
        "HQ4EFgQUi3XVrMsIvg4fZbf6Vr5sp3Xaha8wHwYDVR0jBBgwFoAUi3XVrMsIvg4f"
        "Zbf6Vr5sp3Xaha8wDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOBgQA76Hht"
        "ldY9avcTGSwbwoiuIqv0jTL1fHFnzy3RHMLDh+Lpvolc5DSrSJHCP5WuK0eeJXhr"
        "T5oQpHL9z/cCDLAKCKRa4uV0fhEdOWBqyR9p8y5jJtye72t6CuFUV5iqcpF4BH4f"
        "j2VNHwsSrJwkD4QUGlUtH7vwnQmyCFxZMmWAJg==");
        Vector<uint8_t> certDER;
        base64Decode(certPEM, certDER, WTF::Base64DecodeOptions::Base64Default);
        ssl::unique_ptr<CRYPTO_BUFFER> cert(CRYPTO_BUFFER_new(certDER.data(), certDER.size(), nullptr));
        ASSERT(cert);

        // This is a test key from BoringSSL.
        char kKeyPEM[] =
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "MIICXgIBAAKBgQDYK8imMuRi/03z0K1Zi0WnvfFHvwlYeyK9Na6XJYaUoIDAtB92\n"
        "kWdGMdAQhLciHnAjkXLI6W15OoV3gA/ElRZ1xUpxTMhjP6PyY5wqT5r6y8FxbiiF\n"
        "KKAnHmUcrgfVW28tQ+0rkLGMryRtrukXOgXBv7gcrmU7G1jC2a7WqmeI8QIDAQAB\n"
        "AoGBAIBy09Fd4DOq/Ijp8HeKuCMKTHqTW1xGHshLQ6jwVV2vWZIn9aIgmDsvkjCe\n"
        "i6ssZvnbjVcwzSoByhjN8ZCf/i15HECWDFFh6gt0P5z0MnChwzZmvatV/FXCT0j+\n"
        "WmGNB/gkehKjGXLLcjTb6dRYVJSCZhVuOLLcbWIV10gggJQBAkEA8S8sGe4ezyyZ\n"
        "m4e9r95g6s43kPqtj5rewTsUxt+2n4eVodD+ZUlCULWVNAFLkYRTBCASlSrm9Xhj\n"
        "QpmWAHJUkQJBAOVzQdFUaewLtdOJoPCtpYoY1zd22eae8TQEmpGOR11L6kbxLQsk\n"
        "aMly/DOnOaa82tqAGTdqDEZgSNmCeKKknmECQAvpnY8GUOVAubGR6c+W90iBuQLj\n"
        "LtFp/9ihd2w/PoDwrHZaoUYVcT4VSfJQog/k7kjE4MYXYWL8eEKg3WTWQNECQQDk\n"
        "104Wi91Umd1PzF0ijd2jXOERJU1wEKe6XLkYYNHWQAe5l4J4MWj9OdxFXAxIuuR/\n"
        "tfDwbqkta4xcux67//khAkEAvvRXLHTaa6VFzTaiiO8SaFsHV3lQyXOtMrBpB5jd\n"
        "moZWgjHvB2W9Ckn7sDqsPB+U2tyX0joDdQEyuiMECDY8oQ==\n"
        "-----END RSA PRIVATE KEY-----\n";

        ssl::unique_ptr<BIO> privateKeyBIO(BIO_new_mem_buf(kKeyPEM, strlen(kKeyPEM)));
        ssl::unique_ptr<EVP_PKEY> privateKey(PEM_read_bio_PrivateKey(privateKeyBIO.get(), nullptr, nullptr, nullptr));
        ASSERT(privateKey);

        SSL_CTX_set_chain_and_key(ctx.get(), reinterpret_cast<CRYPTO_BUFFER *const *>(&cert), 1, privateKey.get(), nullptr);

        if (protocol == Protocol::HTTPSWithClientCertificateRequest) {
            SSL_CTX_set_custom_verify(ctx.get(), SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, [] (SSL* ssl, uint8_t*) -> ssl_verify_result_t {
                auto chain = SSL_get0_peer_certificates(ssl);
                EXPECT_EQ(sk_CRYPTO_BUFFER_num(chain), 2u);
                auto cert = sk_CRYPTO_BUFFER_value(chain, 0);
                auto expectedCert = testCertificate();
                EXPECT_EQ(CRYPTO_BUFFER_len(cert), expectedCert.size());
                EXPECT_TRUE(!memcmp(CRYPTO_BUFFER_data(cert), expectedCert.data(), expectedCert.size()));
                return ssl_verify_ok;
            });
        }

        if (maxTLSVersion)
            SSL_CTX_set_max_proto_version(ctx.get(), *maxTLSVersion);

        ssl::unique_ptr<SSL> ssl(SSL_new(ctx.get()));
        ASSERT(ssl);
        SSL_set_fd(ssl.get(), socket);

        auto acceptResult = SSL_accept(ssl.get());
        secureConnectionHandler(acceptResult > 0 ? ssl.get() : nullptr);
    };

    switch (protocol) {
    case Protocol::HTTPS:
    case Protocol::HTTPSWithClientCertificateRequest:
        m_connectionHandler = WTFMove(startSecureConnection);
        break;
    case Protocol::HTTPSProxy:
        m_connectionHandler = [startSecureConnection = WTFMove(startSecureConnection)] (Socket socket) {
            char readBuffer[1000];
            auto bytesRead = ::read(socket, readBuffer, sizeof(readBuffer));
            EXPECT_GT(bytesRead, 0);
            EXPECT_TRUE(static_cast<size_t>(bytesRead) < sizeof(readBuffer));
            
            const char* responseHeader = ""
            "HTTP/1.1 200 Connection Established\r\n"
            "Connection: close\r\n\r\n";
            auto bytesWritten = ::write(socket, responseHeader, strlen(responseHeader));
            EXPECT_EQ(static_cast<size_t>(bytesWritten), strlen(responseHeader));
            startSecureConnection(socket);
        };
        break;
    }
    listenForConnections(1);
}
#endif // HAVE(SSL)

void TCPServer::listenForConnections(size_t connections)
{
    auto listeningSocket = socketBindListen(connections);
    ASSERT(listeningSocket);
    m_listeningThread = std::thread([this, listeningSocket = *listeningSocket, connections] {
        for (size_t i = 0; i < connections; ++i) {
            Socket connectionSocket = accept(listeningSocket, nullptr, nullptr);
            m_connectionThreads.append(std::thread([this, connectionSocket] {
                m_connectionHandler(connectionSocket);
                shutdown(connectionSocket, SHUT_RDWR);
                close(connectionSocket);
            }));
        }
        close(listeningSocket);
    });
}

TCPServer::~TCPServer()
{
    m_listeningThread.join();
    for (auto& connectionThreads : m_connectionThreads)
        connectionThreads.join();
}

auto TCPServer::socketBindListen(size_t connections) -> Optional<Socket>
{
    Socket listeningSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1)
        return WTF::nullopt;
    
    // Ports 49152-65535 are unallocated ports. Try until we find one that's free.
    for (Port port = 49152; port; port++) {
        struct sockaddr_in name;
        memset(&name, 0, sizeof(name));
        name.sin_family = AF_INET;
        name.sin_port = htons(port);
        name.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(listeningSocket, reinterpret_cast<sockaddr*>(&name), sizeof(name)) < 0) {
            // This port is busy. Try the next port.
            continue;
        }
        if (listen(listeningSocket, connections) == -1) {
            // Listening failed.
            close(listeningSocket);
            return WTF::nullopt;
        }
        m_port = port;
        return listeningSocket; // Successfully set up listening port.
    }
    
    // Couldn't find an available port.
    close(listeningSocket);
    return WTF::nullopt;
}

template<> Vector<uint8_t> TCPServer::read(Socket socket)
{
    uint8_t buffer[1000];
    auto bytesRead = ::read(socket, buffer, sizeof(buffer));
    if (bytesRead <= 0)
        return { };
    ASSERT(static_cast<size_t>(bytesRead) < sizeof(buffer));

    Vector<uint8_t> vector;
    vector.append(buffer, bytesRead);
    return vector;
}

template<> void TCPServer::write(Socket socket, const void* response, size_t length)
{
    auto bytesWritten = ::write(socket, response, length);
    EXPECT_EQ(static_cast<size_t>(bytesWritten), length);
}

#if HAVE(SSL)
template<> Vector<uint8_t> TCPServer::read(SSL* ssl)
{
    uint8_t buffer[1000];
    auto bytesRead = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytesRead <= 0)
        return { };
    ASSERT(static_cast<size_t>(bytesRead) < sizeof(buffer));

    Vector<uint8_t> vector;
    vector.append(buffer, bytesRead);
    return vector;
}

template<> void TCPServer::write(SSL* ssl, const void* response, size_t length)
{
    auto bytesWritten = SSL_write(ssl, response, length);
    EXPECT_EQ(static_cast<size_t>(bytesWritten), length);
}
#endif

void TCPServer::respondWithChallengeThenOK(Socket socket)
{
    read(socket);
    
    const char* challengeHeader =
    "HTTP/1.1 401 Unauthorized\r\n"
    "Date: Sat, 23 Mar 2019 06:29:01 GMT\r\n"
    "Content-Length: 0\r\n"
    "WWW-Authenticate: Basic realm=\"testrealm\"\r\n\r\n";
    write(socket, challengeHeader, strlen(challengeHeader));
    
    read(socket);
    
    const char* responseHeader =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 13\r\n\r\n"
    "Hello, World!";
    write(socket, responseHeader, strlen(responseHeader));
}

#if HAVE(SSL)
void TCPServer::respondWithOK(SSL* ssl)
{
    ASSERT(ssl);
    read(ssl);
    
    const char* reply = ""
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 34\r\n\r\n"
    "<script>alert('success!')</script>";
    write(ssl, reply, strlen(reply));
}
#endif

Vector<uint8_t> TCPServer::testCertificate()
{
    // Certificate and private key were generated by running this command:
    // openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
    // and entering this information:
    /*
     Country Name (2 letter code) []:US
     State or Province Name (full name) []:New Mexico
     Locality Name (eg, city) []:Santa Fe
     Organization Name (eg, company) []:Self
     Organizational Unit Name (eg, section) []:Myself
     Common Name (eg, fully qualified host name) []:Me
     Email Address []:me@example.com
     */
    
    String pemEncodedCertificate(""
    "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"
    "3vjZD3y9kZk7mre9XHwkI8MdK5s=");
    
    Vector<uint8_t> vector;
    base64Decode(pemEncodedCertificate, vector, WTF::Base64DecodeOptions::Base64Default);
    return vector;
}

Vector<uint8_t> TCPServer::testPrivateKey()
{
    String pemEncodedPrivateKey(""
    "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"
    "qJfU8bbdbu2pi47Y4FdJK0HLffl5Rw==");

    Vector<uint8_t> vector;
    base64Decode(pemEncodedPrivateKey, vector, WTF::Base64DecodeOptions::Base64Default);
    return vector;
}
    
} // namespace TestWebKitAPI
