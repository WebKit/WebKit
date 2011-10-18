import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

TouchWebView {
    id: webView

    property variant testUrl

    SignalSpy {
        id: spy
        target: webView.page
        signalName: "loadFailed"
    }

    TestCase {
        id: test
        name: "TouchWebViewLoadFail"

        function test_fail() {
            skip("Fails due to https://bugreports.qt.nokia.com/browse/QTBUG-21537")
            compare(spy.count, 0)
            testUrl = Qt.resolvedUrl("file_that_does_not_exist.html")
            webView.page.load(testUrl)
            spy.wait()
            compare(spy.count, 1)
        }
    }
    Connections {
        target: webView.page
        onLoadFailed: {
            test.compare(url, testUrl)
            test.compare(errorCode, NetworkReply.ContentNotFoundError)
            test.compare(errorType, TouchWebPage.NetworkError)
        }
    }
}
