import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0

WebView {
    id: webView
    width: 400
    height: 300

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    ListView {
        id: backItemsList
        anchors.fill: parent
        model: webView.experimental.navigationHistory.backItems
        delegate:
            Text {
                color:"black"
                text: "title : " + title
            }
    }

    ListView {
        id: forwardItemsList
        anchors.fill: parent
        model: webView.experimental.navigationHistory.forwardItems
        delegate:
            Text {
                color:"black"
                text: "title : " + title
            }
    }

    TestCase {
        name: "WebViewNavigationHistory"

        function test_navigationHistory() {
            compare(spy.count, 0)
            compare(webView.loadProgress, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            spy.wait()
            compare(webView.canGoBack, false)
            compare(webView.canGoForward, false)
            compare(backItemsList.count, 0)
            compare(forwardItemsList.count, 0)
            spy.clear()
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/test2.html"))
            compare(webView.canGoBack, true)
            compare(webView.canGoForward, false)
            compare(backItemsList.count, 1)
            spy.clear()
            webView.experimental.goBackTo(0)
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/test1.html"))
            compare(webView.canGoBack, false)
            compare(webView.canGoForward, true)
            compare(backItemsList.count, 0)
            compare(forwardItemsList.count, 1)
            spy.clear()
            webView.goForward()
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/test2.html"))
            compare(webView.canGoBack, true)
            compare(webView.canGoForward, false)
            compare(backItemsList.count, 1)
            compare(forwardItemsList.count, 0)
            spy.clear()
            webView.load(Qt.resolvedUrl("../common/javascript.html"))
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/javascript.html"))
            compare(webView.canGoBack, true)
            compare(webView.canGoForward, false)
            compare(backItemsList.count, 2)
            compare(forwardItemsList.count, 0)
            spy.clear()
            webView.experimental.goBackTo(1)
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/test1.html"))
            compare(webView.canGoBack, false)
            compare(webView.canGoForward, true)
            compare(backItemsList.count, 0)
            compare(forwardItemsList.count, 2)
            spy.clear()
            webView.experimental.goForwardTo(1)
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/javascript.html"))
            compare(webView.canGoBack, true)
            compare(webView.canGoForward, false)
            compare(backItemsList.count, 2)
            compare(forwardItemsList.count, 0)
            spy.clear()
            webView.goBack()
            spy.wait()
            compare(webView.url, Qt.resolvedUrl("../common/test2.html"))
            compare(webView.canGoBack, true)
            compare(webView.canGoForward, true)
            compare(backItemsList.count, 1)
            compare(forwardItemsList.count, 1)
        }
    }
}
