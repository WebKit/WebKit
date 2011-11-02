import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

TouchWebView {
    id: webView
    height: 600
    width: 400

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "TouchWebViewLoad"

        function test_load() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            spy.wait()
            compare(webView.title, "Test page 1")
            compare(webView.width, 400)
            compare(webView.height, 600)
        }
    }
}
