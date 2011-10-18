import QtQuick 2.0
import QtTest 1.0
import QtWebKit.experimental 5.0

DesktopWebView {
    id: webView

    preferences {
        javascriptEnabled: true
        localStorageEnabled: true
        pluginsEnabled: true
    }

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "DesktopWebViewPreferences"

        function init() {
            webView.preferences.javascriptEnabled = true
            webView.preferences.localStorageEnabled = true
            webView.preferences.pluginsEnabled = true
            spy.clear()
        }

        function test_javascriptEnabled() {
            webView.preferences.javascriptEnabled = true
            var testUrl = Qt.resolvedUrl("../common/javascript.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "New Title")
        }

        function test_javascriptDisabled() {
            webView.preferences.javascriptEnabled = false
            var testUrl = Qt.resolvedUrl("../common/javascript.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "Original Title")
        }

        function test_localStorageDisabled() {
            webView.preferences.localStorageEnabled = false
            var testUrl = Qt.resolvedUrl("../common/localStorage.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "Original Title")
        }

        function test_localStorageEnabled() {
            webView.preferences.localStorageEnabled = true
            var testUrl = Qt.resolvedUrl("../common/localStorage.html")
            webView.load(testUrl)
            spy.wait()
            spy.clear()
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "New Title")
        }

        function test_pluginsDisabled() {
            webView.preferences.pluginsEnabled = false
            var testUrl = Qt.resolvedUrl("../common/plugins.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "Plugins Not Loaded")
        }

        function test_pluginsEnabled() {
            webView.preferences.pluginsEnabled = true
            var testUrl = Qt.resolvedUrl("../common/plugins.html")
            webView.load(testUrl)
            spy.wait()
            spy.clear()
            webView.load(testUrl)
            spy.wait()
            compare(webView.title, "Original Title")
        }
    }
}
