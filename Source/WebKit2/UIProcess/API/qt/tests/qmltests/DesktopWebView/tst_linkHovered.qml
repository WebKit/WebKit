import QtQuick 2.0
import QtTest 1.0
import QtWebKit.experimental 5.0

DesktopWebView {
    id: webView
    width: 200
    height: 400

    property string lastUrl
    property string lastTitle

    SignalSpy {
        id: spy
        target: webView
        signalName: "linkHovered"
    }

    SignalSpy {
        id: loadSpy
        target: webView
        signalName: "loadSucceeded"
    }

    onLinkHovered: {
        webView.lastUrl = url
        webView.lastTitle = title
    }

    TestCase {
        name: "DesktopWebViewLinkHovered"
        when: windowShown

        function init() {
            webView.lastUrl = ""
            webView.lastTitle = ""
            spy.clear()
        }

        function test_linkHovered() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            loadSpy.wait()
            mouseMove(webView, 100, 100)
            spy.wait()
            compare(spy.count, 1)
            compare(webView.lastUrl, Qt.resolvedUrl("../common/test1.html"))
            compare(webView.lastTitle, "A title")
            mouseMove(webView, 100, 300)
            spy.wait()
            compare(spy.count, 2)
            compare(webView.lastUrl, "")
            compare(webView.lastTitle, "")
        }

        function test_linkHoveredDoesntEmitRepeated() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            loadSpy.wait()

            for (var i = 0; i < 100; i += 10)
                mouseMove(webView, 100, 100 + i)

            tryCompare(spy.count, 1)

            for (var i = 0; i < 100; i += 10)
                mouseMove(webView, 100, 300 + i)

            spy.wait()
            tryCompare(spy.count, 2)
            compare(webView.lastUrl, "")
        }
    }
}
