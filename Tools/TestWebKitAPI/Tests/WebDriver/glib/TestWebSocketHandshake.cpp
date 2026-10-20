/*
 * Copyright (C) 2026 Gitstart
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TestMain.h"
#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(GTK)
static constexpr auto webDriverExecutableName = "WebKitWebDriver";
#elif PLATFORM(WPE)
static constexpr auto webDriverExecutableName = "WPEWebDriver";
#endif

static uint16_t findUnusedPort()
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GSocket> socket = adoptGRef(g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_TCP, &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_nonnull(socket.get());

    GRefPtr<GInetAddress> loopbackAddress = adoptGRef(g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4));
    GRefPtr<GSocketAddress> socketAddress = adoptGRef(g_inet_socket_address_new(loopbackAddress.get(), 0));
    g_assert_true(g_socket_bind(socket.get(), socketAddress.get(), FALSE, &error.outPtr()));
    g_assert_no_error(error.get());

    GRefPtr<GSocketAddress> boundAddress = adoptGRef(g_socket_get_local_address(socket.get(), &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_true(G_IS_INET_SOCKET_ADDRESS(boundAddress.get()));
    auto port = g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(boundAddress.get()));
    g_assert_cmpuint(port, >, 0);

    g_assert_true(g_socket_close(socket.get(), &error.outPtr()));
    g_assert_no_error(error.get());
    return port;
}

class WebDriverProcess {
public:
    explicit WebDriverProcess(const char* allowedOrigins = nullptr)
        : m_bidiPort(findUnusedPort())
    {
        GUniquePtr<char> executablePath(g_build_filename(WEBKIT_EXEC_PATH, webDriverExecutableName, nullptr));
        GUniquePtr<char> portArgument(g_strdup("--port=0"));
        GUniquePtr<char> bidiPortArgument(g_strdup_printf("--bidi-port=%u", m_bidiPort));
        GUniquePtr<char> allowedOriginsArgument(allowedOrigins ? g_strdup_printf("--allowed-origins=%s", allowedOrigins) : nullptr);
        const char* arguments[] = {
            executablePath.get(),
            portArgument.get(),
            bidiPortArgument.get(),
            allowedOriginsArgument.get(),
            nullptr,
        };

        GUniqueOutPtr<GError> error;
        m_process = adoptGRef(g_subprocess_newv(arguments, G_SUBPROCESS_FLAGS_NONE, &error.outPtr()));
        g_assert_no_error(error.get());
        g_assert_nonnull(m_process.get());
        waitUntilListening();
    }

    ~WebDriverProcess()
    {
        g_subprocess_force_exit(m_process.get());
        GUniqueOutPtr<GError> error;
        g_assert_true(g_subprocess_wait(m_process.get(), nullptr, &error.outPtr()));
        g_assert_no_error(error.get());
    }

    unsigned handshakeStatus(const char* requestTarget = "/session", const char* host = nullptr, const char* origin = nullptr, bool includeWebSocketKey = true)
    {
        GUniquePtr<char> defaultHost;
        if (!host) {
            defaultHost.reset(g_strdup_printf("127.0.0.1:%u", m_bidiPort));
            host = defaultHost.get();
        }

        GUniquePtr<char> originHeader(origin ? g_strdup_printf("Origin: %s\r\n", origin) : g_strdup(""));
        GUniquePtr<char> request(g_strdup_printf(
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "%s"
            "Sec-WebSocket-Version: 13\r\n"
            "%s"
            "\r\n",
            requestTarget,
            host,
            includeWebSocketKey ? "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" : "",
            originHeader.get()));
        return sendRequest(request.get());
    }

    unsigned bidiPort() const { return m_bidiPort; }

private:
    GRefPtr<GSocketConnection> connect() const
    {
        GRefPtr<GSocketClient> client = adoptGRef(g_socket_client_new());
        g_socket_client_set_enable_proxy(client.get(), FALSE);
        g_socket_client_set_timeout(client.get(), 2);
        GUniqueOutPtr<GError> error;
        return adoptGRef(g_socket_client_connect_to_host(client.get(), "127.0.0.1", m_bidiPort, nullptr, &error.outPtr()));
    }

    void waitUntilListening()
    {
        static constexpr unsigned maximumAttempts = 100;
        for (unsigned attempt = 0; attempt < maximumAttempts; ++attempt) {
            if (auto connection = connect()) {
                g_io_stream_close(G_IO_STREAM(connection.get()), nullptr, nullptr);
                return;
            }
            g_usleep(10 * G_TIME_SPAN_MILLISECOND);
        }
        g_error("WebDriver did not listen on the BiDi port");
    }

    unsigned sendRequest(const char* request) const
    {
        auto connection = connect();
        g_assert_nonnull(connection.get());
        g_socket_set_timeout(g_socket_connection_get_socket(connection.get()), 2);

        GUniqueOutPtr<GError> error;
        gsize bytesWritten;
        auto requestLength = strlen(request);
        g_assert_true(g_output_stream_write_all(g_io_stream_get_output_stream(G_IO_STREAM(connection.get())), request, requestLength, &bytesWritten, nullptr, &error.outPtr()));
        g_assert_no_error(error.get());
        g_assert_cmpuint(bytesWritten, ==, requestLength);

        GRefPtr<GDataInputStream> input = adoptGRef(g_data_input_stream_new(g_io_stream_get_input_stream(G_IO_STREAM(connection.get()))));
        g_data_input_stream_set_newline_type(input.get(), G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
        GUniquePtr<char> statusLine(g_data_input_stream_read_line(input.get(), nullptr, nullptr, &error.outPtr()));
        g_assert_no_error(error.get());
        g_assert_nonnull(statusLine.get());
        g_assert_true(g_str_has_prefix(statusLine.get(), "HTTP/1.1 "));

        auto* statusStart = statusLine.get() + strlen("HTTP/1.1 ");
        char* end = nullptr;
        auto status = g_ascii_strtoull(statusStart, &end, 10);
        g_assert_true(end != statusStart);
        g_assert_cmpuint(status, >=, 100);
        g_assert_cmpuint(status, <=, 599);
        g_io_stream_close(G_IO_STREAM(connection.get()), nullptr, nullptr);
        return status;
    }

    GRefPtr<GSubprocess> m_process;
    uint16_t m_bidiPort { 0 };
};

static void testNoOriginIsAllowed()
{
    WebDriverProcess driver;
    g_assert_cmpuint(driver.handshakeStatus(), ==, 101);
}

static void testOriginIsDeniedByDefault()
{
    WebDriverProcess driver;
    g_assert_cmpuint(driver.handshakeStatus("/session", nullptr, "https://example.com"), ==, 403);
}

static void testAllowlistedOriginIsAllowed()
{
    WebDriverProcess driver("https://example.com");
    g_assert_cmpuint(driver.handshakeStatus("/session", nullptr, "https://example.com"), ==, 101);
}

static void testInvalidOriginsAreDenied()
{
    WebDriverProcess driver("https://example.com");
    g_assert_cmpuint(driver.handshakeStatus("/session", nullptr, "file://example.com"), ==, 403);
    g_assert_cmpuint(driver.handshakeStatus("/session", nullptr, "https://example.com/path"), ==, 403);
    g_assert_cmpuint(driver.handshakeStatus("/session", nullptr, "https://\xC3\xA9" "xample.com"), ==, 403);
}

static void testDisallowedHostIsDenied()
{
    WebDriverProcess driver;
    g_assert_cmpuint(driver.handshakeStatus("/session", "example.com"), ==, 403);
}

static void testMalformedHostIsDenied()
{
    WebDriverProcess driver;
    g_assert_cmpuint(driver.handshakeStatus("/session", "localhost@evil.example"), ==, 403);
}

static void testAbsoluteRequestTargetHostMismatchIsDenied()
{
    WebDriverProcess driver;
    GUniquePtr<char> host(g_strdup_printf("127.0.0.1:%u", driver.bidiPort()));
    g_assert_cmpuint(driver.handshakeStatus("http://example.com/session", host.get()), ==, 403);
}

static void testMalformedWebSocketHandshakeIsRejected()
{
    WebDriverProcess driver;
    g_assert_cmpuint(driver.handshakeStatus("/session", nullptr, nullptr, false), ==, 400);
}

static void testUnknownResourceIsRejected()
{
    WebDriverProcess driver;
    g_assert_cmpuint(driver.handshakeStatus("/unknown"), ==, 503);
}

void beforeAll()
{
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/no-origin-is-allowed", testNoOriginIsAllowed);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/origin-is-denied-by-default", testOriginIsDeniedByDefault);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/allowlisted-origin-is-allowed", testAllowlistedOriginIsAllowed);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/invalid-origins-are-denied", testInvalidOriginsAreDenied);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/disallowed-host-is-denied", testDisallowedHostIsDenied);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/malformed-host-is-denied", testMalformedHostIsDenied);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/absolute-request-target-host-mismatch-is-denied", testAbsoluteRequestTargetHostMismatchIsDenied);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/malformed-websocket-handshake-is-rejected", testMalformedWebSocketHandshakeIsRejected);
    g_test_add_func("/webkit/WebDriver/WebSocketHandshake/unknown-resource-is-rejected", testUnknownResourceIsRejected);
}

void afterAll()
{
}
