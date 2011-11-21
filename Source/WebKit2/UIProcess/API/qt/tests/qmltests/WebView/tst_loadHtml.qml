import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView
    width: 200
    height: 400

    SignalSpy {
        id: loadSpy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "WebViewLoadHtml"

        function test_loadProgressAfterLoadHtml() {
            loadSpy.clear()
            compare(loadSpy.count, 0)
            compare(webView.loadProgress, 0)
            webView.loadHtml("<html><head><title>Test page 1</title></head><body>Hello.</body></html>")
            loadSpy.wait()
            compare(webView.loadProgress, 100)
        }
    }
}
