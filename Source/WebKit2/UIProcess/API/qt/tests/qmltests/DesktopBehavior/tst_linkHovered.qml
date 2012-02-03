import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView
    width: 200
    height: 400
    focus: true

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

        // Delayed windowShown to workaround problems with Qt5 in debug mode.
        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

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

            spy.wait()
            compare(spy.count, 1)
            compare(webView.lastUrl, Qt.resolvedUrl("../common/test1.html"))

            for (var i = 0; i < 100; i += 10)
                mouseMove(webView, 100, 300 + i)

            spy.wait()
            compare(spy.count, 2)
            compare(webView.lastUrl, "")
        }
    }
}
