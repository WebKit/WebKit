import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKitTest 1.0

Item {
    DesktopWebView {
        id: webView
        width: 200
        height: 200
        onNavigationRequested: {
            if (request.button == Qt.MiddleButton && request.modifiers & Qt.ControlModifier) {
                otherWebView.load(request.url)
                request.action = WebView.IgnoreRequest
            }
        }
    }

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    DesktopWebView {
        id: otherWebView
    }

    SignalSpy {
        id: otherSpy
        target: otherWebView
        signalName: "loadSucceeded"
    }

    TestCase {
        name: "DesktopWebViewNavigationRequested"

        // Delayed windowShown to workaround problems with Qt5 in debug mode.
        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

        function test_usePolicy() {
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            spy.wait()
            spy.clear()
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
            compare(webView.title, "Test page 1")
        }

        function test_ignorePolicy() {
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            spy.wait()
            spy.clear()
            compare(spy.count, 0)
            compare(otherSpy.count, 0)
            mouseClick(webView, 100, 100, Qt.MiddleButton, Qt.ControlModifier)
            otherSpy.wait()
            compare(spy.count, 0)
            compare(otherSpy.count, 1)
            compare(otherWebView.title, "Test page 1")
        }
    }
}
