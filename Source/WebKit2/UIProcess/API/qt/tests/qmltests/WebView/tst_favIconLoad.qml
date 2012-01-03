import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView
        signalName: "iconChanged"
    }

    SignalSpy {
        id: loadSpy
        target: webView
        signalName: "loadSucceeded"
    }

    Image {
        id: favicon
        source: webView.icon
    }

    TestCase {
        id: test
        name: "WebViewLoadFavIcon"

        function init() {
            if (webView.icon != '') {
                // If this is not the first test, then load a blank page without favicon, restoring the initial state.
                webView.load('about:blank')
                spy.wait()
                loadSpy.wait()
            }
            loadSpy.clear()
            spy.clear()
        }

        function test_favIconLoad() {
            compare(spy.count, 0)
            var url = Qt.resolvedUrl("../common/favicon.html")
            webView.load(url)
            spy.wait()
            compare(spy.count, 1)
            compare(favicon.width, 48)
            compare(favicon.height, 48)
        }

        function test_favIconLoadEncodedUrl() {
            compare(spy.count, 0)
            var url = Qt.resolvedUrl("../common/favicon2.html?favicon=load should work with#whitespace!")
            webView.load(url)
            spy.wait()
            compare(spy.count, 1)
            compare(favicon.width, 16)
            compare(favicon.height, 16)
        }
    }
}
