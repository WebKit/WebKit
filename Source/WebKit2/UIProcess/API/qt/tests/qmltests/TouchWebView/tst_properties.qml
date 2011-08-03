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
        name: "TouchWebViewProperties"

        function test_title() {
            compare(spy.count, 0)
            webView.page.load(Qt.resolvedUrl("../common/test1.html"))
            spy.wait()
            compare(webView.page.title, "Test page 1")
        }
    }
}
