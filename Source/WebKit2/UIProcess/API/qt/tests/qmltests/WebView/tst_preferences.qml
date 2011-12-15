import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 3.0

WebView {
    id: webView
    width: 400
    height: 300

    experimental.preferences.javascriptEnabled: true
    experimental.preferences.localStorageEnabled: true
    experimental.preferences.pluginsEnabled: true

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "WebViewPreferences"

        function init() {
            webView.experimental.preferences.javascriptEnabled = true
            webView.experimental.preferences.localStorageEnabled = true
            webView.experimental.preferences.pluginsEnabled = true
            spy.clear()
        }

        function test_javascriptEnabled() {
            webView.experimental.preferences.javascriptEnabled = true
            var testUrl = Qt.resolvedUrl("../common/javascript.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "New Title")
        }

        function test_javascriptDisabled() {
            webView.experimental.preferences.javascriptEnabled = false
            var testUrl = Qt.resolvedUrl("../common/javascript.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "Original Title")
        }

        function test_localStorageDisabled() {
            webView.experimental.preferences.localStorageEnabled = false
            var testUrl = Qt.resolvedUrl("../common/localStorage.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "Original Title")
        }

        function test_localStorageEnabled() {
            webView.experimental.preferences.localStorageEnabled = true
            var testUrl = Qt.resolvedUrl("../common/localStorage.html")
            webView.load(testUrl)
            spy.wait()
            spy.clear()
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "New Title")
        }
    }
}
