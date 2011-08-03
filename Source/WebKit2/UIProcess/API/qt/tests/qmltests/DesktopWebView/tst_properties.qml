import QtQuick 2.0
import QtTest 1.0
import QtWebKit.experimental 5.0

DesktopWebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "DesktopWebViewProperties"

        function test_title() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            spy.wait()
            compare(webView.title, "Test page 1")
        }
    }
}
