import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView
    width: 400
    height: 300

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "WebViewProperties"

        function test_title() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            spy.wait()
            compare(webView.title, "Test page 1")
        }

        function test_url() {
            compare(spy.count, 1)
            var testUrl = Qt.resolvedUrl("../common/test1.html")
            webView.load(testUrl)
            spy.wait()
            compare(webView.url, testUrl)
        }
    }
}
