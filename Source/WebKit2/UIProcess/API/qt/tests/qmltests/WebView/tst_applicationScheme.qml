import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 3.0
import Test 1.0

WebView {
    id: webView
    width: 400
    height: 300

    experimental {
        urlSchemeDelegates: [
            UrlSchemeDelegate {
                scheme: "applicationScheme"
                onReceivedRequest: {
                    reply.data = "<html><head><title>Test Application Scheme</title></head><body>A test page.</body></html>"
                    reply.send()
                }
            },
            UrlSchemeDelegate {
                scheme: "scheme1"
                onReceivedRequest: {
                    reply.data = "<html><head><title>Scheme1 Reply</title></head><body>A test page.</body></html>"
                    reply.send()
                }
            },
            UrlSchemeDelegate {
                scheme: "scheme2"
                onReceivedRequest: {
                    reply.data = "<html><head><title>Scheme2 Reply</title></head><body>A test page.</body></html>"
                    reply.send()
                }
            },
            UrlSchemeDelegate {
                scheme: "scheme3"
                onReceivedRequest: {
                    if (request.url == "scheme3://url1")
                        reply.data = "<html><head><title>Scheme3 Reply1</title></head><body>A test page.</body></html>"
                    else if (request.url == "scheme3://url2")
                        reply.data = "<html><head><title>Scheme3 Reply2</title></head><body>A test page.</body></html>"
                    else
                        reply.data = "<html><head><title>Should not happen</title></head><body>A test page.</body></html>"
                    reply.send()
                }
            },
            UrlSchemeDelegate {
                scheme: "schemeCharset"
                onReceivedRequest: {
                    if (request.url == "schemecharset://latin1") {
                        reply.data = byteArrayHelper.latin1Data
                        reply.contentType = "text/html; charset=iso-8859-1"
                    } else if (request.url == "schemecharset://utf-8") {
                        reply.data = byteArrayHelper.utf8Data
                        reply.contentType = "text/html; charset=utf-8"
                    }
                    reply.send()
                }
            }

        ]
    }

    SignalSpy {
        id: spyTitle
        target: webView
        signalName: "titleChanged"
    }

    ByteArrayTestData {
        id: byteArrayHelper
    }

    TestCase {
        name: "WebViewApplicationSchemes"

        function test_applicationScheme() {
            spyTitle.clear()
            compare(spyTitle.count, 0)
            var testUrl = "applicationScheme://something"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "Test Application Scheme")
        }

        function test_multipleSchemes() {
            // Test if we receive the right reply when defining multiple schemes.
            spyTitle.clear()
            compare(spyTitle.count, 0)
            var testUrl = "scheme2://some-url-string"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "Scheme2 Reply")

            testUrl = "scheme1://some-url-string"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "Scheme1 Reply")

            compare(spyTitle.count, 2)
        }

        function test_multipleUrlsForScheme() {
            spyTitle.clear()
            compare(spyTitle.count, 0)
            var testUrl = "scheme3://url1"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "Scheme3 Reply1")

            testUrl = "scheme3://url2"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "Scheme3 Reply2")

            compare(spyTitle.count, 2)
        }

        function test_charsets() {
            spyTitle.clear()
            compare(spyTitle.count, 0)
            var testUrl = "schemeCharset://latin1"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "title with copyright ©")

            testUrl = "schemeCharset://utf-8"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "title with copyright ©")
        }
    }
}
