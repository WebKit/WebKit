import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKitTest 1.0

DesktopWebView {
    id: webView
    width: 200
    height: 400

    property string lastUrl

    SignalSpy {
        id: loadSpy
        target: webView
        signalName: "loadSucceeded"
    }

    SignalSpy {
        id: linkHoveredSpy
        target: webView
        signalName: "linkHovered"
    }

    onLinkHovered: {
        webView.lastUrl = url
    }

    TestCase {
        name: "DesktopWebViewLoadHtml"

        function init() {
            webView.lastUrl = ""
            linkHoveredSpy.clear()
            loadSpy.clear()
        }

        function test_baseUrlAfterLoadHtml() {
            skip("Link Hovered is currently not working")
            loadSpy.clear()
            linkHoveredSpy.clear()
            compare(linkHoveredSpy.count, 0)
            webView.loadHtml("<html><head><title>Test page with huge link area</title></head><body><a title=\"A title\" href=\"test1.html\"><img width=200 height=200></a></body></html>", "http://www.example.foo.com")
            loadSpy.wait()
            compare("http://www.example.foo.com/", webView.url)
            mouseMove(webView, 100, 100)
            linkHoveredSpy.wait()
            compare(linkHoveredSpy.count, 1)
            compare(webView.lastUrl, "http://www.example.foo.com/test1.html")
        }
    }
}
