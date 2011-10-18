import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

DesktopWebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "DesktopWebViewLoadProgress"

        function test_loadProgress() {
            compare(spy.count, 0)
            compare(webView.loadProgress, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            compare(webView.loadProgress, 0)
            spy.wait()
            compare(webView.loadProgress, 100)
        }
    }
}
