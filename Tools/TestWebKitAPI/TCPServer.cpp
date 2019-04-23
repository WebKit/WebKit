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

extern "C" {

struct BIO;
struct X509;
struct SSL_CTX;
struct EVP_PKEY;
struct SSL_METHOD;
struct pem_password_cb;
int BIO_free(BIO*);
int SSL_free(SSL*);
int X509_free(X509*);
int SSL_CTX_free(SSL_CTX*);
int EVP_PKEY_free(EVP_PKEY*);
int SSL_library_init();
const SSL_METHOD* SSLv23_server_method();
BIO* BIO_new_mem_buf(const void*, int);
X509* PEM_read_bio_X509(BIO*, X509**, pem_password_cb*, void*);
EVP_PKEY* PEM_read_bio_PrivateKey(BIO*, EVP_PKEY**, pem_password_cb*, void*);
SSL_CTX* SSL_CTX_new(const SSL_METHOD*);
const SSL_METHOD* SSLv23_server_method();
int SSL_CTX_use_certificate(SSL_CTX*, X509*);
int SSL_CTX_use_PrivateKey(SSL_CTX*, EVP_PKEY*);
SSL* SSL_new(SSL_CTX*);
int SSL_accept(SSL*);
int SSL_set_fd(SSL*, int);

} // extern "C"

namespace TestWebKitAPI {

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
template<> struct deleter<X509> {
    void operator()(X509* x509)
    {
        X509_free(x509);
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

TCPServer::TCPServer(Function<void(Socket)>&& connectionHandler, size_t connections)
    : m_connectionHandler(WTFMove(connectionHandler))
{
    listenForConnections(connections);
}

TCPServer::TCPServer(Function<void(SSL*)>&& secureConnectionHandler)
    : m_connectionHandler([secureConnectionHandler = WTFMove(secureConnectionHandler)] (Socket socket) {

        SSL_library_init();

        std::unique_ptr<SSL_CTX, deleter<SSL_CTX>> ctx(SSL_CTX_new(SSLv23_server_method()));

        // This is a test certificate from BoringSSL.
        char kCertPEM[] =
        "-----BEGIN CERTIFICATE-----\n"
        "MIICWDCCAcGgAwIBAgIJAPuwTC6rEJsMMA0GCSqGSIb3DQEBBQUAMEUxCzAJBgNV\n"
        "BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX\n"
        "aWRnaXRzIFB0eSBMdGQwHhcNMTQwNDIzMjA1MDQwWhcNMTcwNDIyMjA1MDQwWjBF\n"
        "MQswCQYDVQQGEwJBVTETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50\n"
        "ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKB\n"
        "gQDYK8imMuRi/03z0K1Zi0WnvfFHvwlYeyK9Na6XJYaUoIDAtB92kWdGMdAQhLci\n"
        "HnAjkXLI6W15OoV3gA/ElRZ1xUpxTMhjP6PyY5wqT5r6y8FxbiiFKKAnHmUcrgfV\n"
        "W28tQ+0rkLGMryRtrukXOgXBv7gcrmU7G1jC2a7WqmeI8QIDAQABo1AwTjAdBgNV\n"
        "HQ4EFgQUi3XVrMsIvg4fZbf6Vr5sp3Xaha8wHwYDVR0jBBgwFoAUi3XVrMsIvg4f\n"
        "Zbf6Vr5sp3Xaha8wDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQUFAAOBgQA76Hht\n"
        "ldY9avcTGSwbwoiuIqv0jTL1fHFnzy3RHMLDh+Lpvolc5DSrSJHCP5WuK0eeJXhr\n"
        "T5oQpHL9z/cCDLAKCKRa4uV0fhEdOWBqyR9p8y5jJtye72t6CuFUV5iqcpF4BH4f\n"
        "j2VNHwsSrJwkD4QUGlUtH7vwnQmyCFxZMmWAJg==\n"
        "-----END CERTIFICATE-----\n";

        std::unique_ptr<BIO, deleter<BIO>> certBIO(BIO_new_mem_buf(kCertPEM, strlen(kCertPEM)));
        std::unique_ptr<X509, deleter<X509>> certX509(PEM_read_bio_X509(certBIO.get(), nullptr, nullptr, nullptr));
        ASSERT(certX509);
        SSL_CTX_use_certificate(ctx.get(), certX509.get());

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

        std::unique_ptr<BIO, deleter<BIO>> privateKeyBIO(BIO_new_mem_buf(kKeyPEM, strlen(kKeyPEM)));
        std::unique_ptr<EVP_PKEY, deleter<EVP_PKEY>> privateKey(PEM_read_bio_PrivateKey(privateKeyBIO.get(), nullptr, nullptr, nullptr));
        ASSERT(privateKey);
        SSL_CTX_use_PrivateKey(ctx.get(), privateKey.get());

        std::unique_ptr<SSL, deleter<SSL>> ssl(SSL_new(ctx.get()));
        ASSERT(ssl);
        SSL_set_fd(ssl.get(), socket);

        auto acceptResult = SSL_accept(ssl.get());
        ASSERT_UNUSED(acceptResult, acceptResult > 0);
        
        secureConnectionHandler(ssl.get());
    })
{
    listenForConnections(1);
}

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

} // namespace TestWebKitAPI
