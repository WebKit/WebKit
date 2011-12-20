import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView
    width: 400
    height: 300

    property variant testUrl

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadFailed"
    }

    TestCase {
        id: test
        name: "WebViewLoadFail"
        function test_fail() {
            compare(spy.count, 0)
            testUrl = Qt.resolvedUrl("file_that_does_not_exist.html")
            webView.load(testUrl)
            spy.wait()
            compare(spy.count, 1)
        }
    }

    onLoadFailed: {
        test.compare(url, testUrl)
        test.compare(errorCode, NetworkReply.ContentNotFoundError)
        test.compare(errorDomain, WebView.NetworkErrorDomain)
    }
}
