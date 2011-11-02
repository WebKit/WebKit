import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

TouchWebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "TouchWebViewLoadProgress"

        function test_loadProgress() {
            compare(spy.count, 0)
            var testUrl = Qt.resolvedUrl("../common/test1.html")
            webView.load(testUrl)
            compare(webView.loadProgress, 0)
            spy.wait()
            compare(webView.loadProgress, 100)
        }
    }
}
