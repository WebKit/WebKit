import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

DesktopWebView {
    id: webView
    height: 0
    width: 0

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "DesktopWebViewLoad"

        function test_loadZeroSizeView() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            spy.wait()
            compare(webView.title, "Test page 1")
            compare(webView.width, 0)
            compare(webView.height, 0)
        }
    }
}
