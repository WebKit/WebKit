import QtQuick 2.0
import QtTest 1.0
import QtWebKit.experimental 5.0

TouchWebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView.page
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "TouchWebViewLoadProgress"

        function test_loadProgress() {
            compare(spy.count, 0)
            var testUrl = Qt.resolvedUrl("../common/test1.html")
            webView.page.load(testUrl)
            compare(webView.page.loadProgress, 0)
            spy.wait()
            compare(webView.page.loadProgress, 100)
        }
    }
}
