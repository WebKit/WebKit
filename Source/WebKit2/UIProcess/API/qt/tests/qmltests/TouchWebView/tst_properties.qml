import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

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

        function test_url() {
            compare(spy.count, 1)
            var testUrl = Qt.resolvedUrl("../common/test1.html")
            webView.page.load(testUrl)
            spy.wait()
            compare(webView.page.url, testUrl)
        }

    }
}
