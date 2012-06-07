import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0
import "../common"

Item {
    TestWebView {
        id: webView
        property variant lastResult
    }

    SignalSpy {
        id: resultSpy
        target: webView
        signalName: "lastResultChanged"
    }

    TestCase {
        name: "DevicePixelRatio"

        function init() {
            resultSpy.clear()
            webView.lastResult = null
        }

        function test_devicePixelRatio() {
            expectFail("", "This currently fails since window.devicePixelRatio doesn't return the updated value.")

            resultSpy.clear()
            webView.url = "about:blank"
            webView.experimental.devicePixelRatio = 2.0
            verify(webView.waitForLoadSucceeded())

            webView.experimental.evaluateJavaScript(
                "(function() { return window.devicePixelRatio })()",
                function(result) {
                    webView.lastResult = result
                })

            resultSpy.wait()
            compare(webView.lastResult, 2.0)
            compare(webView.lastResult, webView.experimental.devicePixelRatio)
        }

        function test_devicePixelRatioMediaQuery() {
            expectFail("", "This currently fails since the devicePixelRatio from the QML API isn't propagated correctly.")
            resultSpy.clear()
            webView.url = "about:blank"
            webView.experimental.devicePixelRatio = 2.0
            verify(webView.waitForLoadSucceeded())

            webView.experimental.evaluateJavaScript(
                "(function() { return window.matchMedia('-webkit-device-pixel-ratio: 2').matches })()",
                function(result) {
                    webView.lastResult = result
                })

            resultSpy.wait()
            verify(webView.lastResult)
        }
    }
}
