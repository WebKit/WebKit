import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0
import "../common"

Item {
    TestWebView {
        id: webView
        property variant lastMessage
        property variant lastResult

        signal resultReceived

        experimental.preferences.navigatorQtObjectEnabled: true
        experimental.onMessageReceived: {
            lastMessage = message
        }
    }

    SignalSpy {
        id: messageSpy
        target: webView.experimental
        signalName: "messageReceived"
    }

    SignalSpy {
        id: resultSpy
        target: webView
        signalName: "resultReceived"
    }

    TestCase {
        name: "JavaScriptEvaluation"

        function init() {
            messageSpy.clear()
            webView.lastMessage = null

            resultSpy.clear()
            webView.lastResult = null
        }

        function test_basic() {
            messageSpy.clear()
            webView.url = "about:blank"
            verify(webView.waitForLoadSucceeded())

            webView.experimental.evaluateJavaScript(
                "navigator.qt.onmessage = function(message) {" +
                "    var result = message.data.split('');" +
                "    result = result.reverse().join('');" +
                "    navigator.qt.postMessage(result);" +
                "}");

            webView.experimental.postMessage("DLROW OLLEH");
            messageSpy.wait()
            compare(webView.lastMessage.data, "HELLO WORLD")
        }

        function test_propertyObjectWithChild() {
            messageSpy.clear()
            webView.url = "about:blank"
            verify(webView.waitForLoadSucceeded())

            webView.experimental.evaluateJavaScript(
                "(function() {" +
                "    var parent = new Object;" +
                "    var child = new Object;" +
                "    parent['level'] = '1';" +
                "    child['level'] = 2;" +
                "    parent['child'] = child;" +
                "    return parent;" +
                "})()",

                function(result) {
                    webView.lastResult = result
                    webView.resultReceived()
                });

            resultSpy.wait()

            compare(JSON.stringify(webView.lastResult),
                '{"child":{"level":2},"level":"1"}')
        }
    }
}
