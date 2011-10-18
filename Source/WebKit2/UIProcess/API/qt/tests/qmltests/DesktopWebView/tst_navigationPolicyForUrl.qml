import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

Item {
    DesktopWebView {
        id: webView
        width: 200
        height: 200
        function navigationPolicyForUrl(url, button, modifiers) {
            if (button == Qt.MiddleButton && modifiers & Qt.ControlModifier) {
                otherWebView.load(url)
                return DesktopWebView.IgnorePolicy
            }
            return DesktopWebView.UsePolicy
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
        name: "DesktopWebViewNavigationPolicyForUrl"
        when: windowShown

        function test_usePolicy() {
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            spy.wait()
            spy.clear()
            compare(spy.count, 0)
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
