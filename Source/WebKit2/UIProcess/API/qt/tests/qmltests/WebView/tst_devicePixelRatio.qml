import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0
import "../common"


TestWebView {
    id: webView
    property variant lastResult
    width: 400
    height: 300
    focus: true

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
            resultSpy.clear()
            webView.url = Qt.resolvedUrl("../common/test1.html");
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
            resultSpy.clear()
            webView.url = Qt.resolvedUrl("../common/test2.html");
            webView.experimental.devicePixelRatio = 2.0
            verify(webView.waitForLoadSucceeded())

            webView.experimental.evaluateJavaScript(
                "(function() { return window.matchMedia(\"(-webkit-device-pixel-ratio: 2)\").matches })()",
                function(result) {
                    webView.lastResult = result
                })

            resultSpy.wait()
            verify(webView.lastResult)
        }
    }
}
