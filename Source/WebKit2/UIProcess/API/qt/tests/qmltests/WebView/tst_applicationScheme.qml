import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 3.0

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
            }
        ]
    }

    SignalSpy {
        id: spyTitle
        target: webView
        signalName: "titleChanged"
    }

    TestCase {
        name: "WebViewApplicationSchemes"

        function test_applicationScheme() {
            compare(spyTitle.count, 0)
            var testUrl = "applicationScheme://something"
            webView.load(testUrl)
            spyTitle.wait()
            compare(webView.title, "Test Application Scheme")
        }
    }

}
