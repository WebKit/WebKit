import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0
import Test 1.0
import "../common"

Item {
    TestWebView {
        id: webView
        width: 480
        height: 720

        property variant result

        experimental.test.onContentsScaleCommitted: scaleChanged()

        property variant content: "data:text/html," +
            "<head>" +
            "    <meta name='viewport' content='width=device-width'>" +
            "</head>" +
            "<body>" +
            "    <div id='target' " +
            "         style='position:absolute; left:20; top:20; width:200; height:200;'>" +
            "    </div>" +
            "</body>"

        signal resultReceived
        signal scaleChanged
    }

    SignalSpy {
        id: resultSpy
        target: webView
        signalName: "resultReceived"
    }

    SignalSpy {
        id: scaleSpy
        target: webView
        signalName: "scaleChanged"
    }

    TestCase {
        name: "DoubleTapToZoom"

        property variant test: webView.experimental.test

        // Delayed windowShown to workaround problems with Qt5 in debug mode.
        when: false
        Timer {
            running: parent.windowShown
            repeat: false
            interval: 1
            onTriggered: parent.when = true
        }

        function init() {
            resultSpy.clear()
            scaleSpy.clear()
        }

        function documentSize() {
            resultSpy.clear();
            var result;

             webView.experimental.evaluateJavaScript(
                "window.innerWidth + 'x' + window.innerHeight",
                function(size) { webView.resultReceived(); result = size });
            resultSpy.wait();
            return result;
        }

        function elementRect(id) {
            resultSpy.clear();
            var result;

             webView.experimental.evaluateJavaScript(
                "document.getElementById('" + id + "').getBoundingClientRect();",
                function(rect) { webView.resultReceived(); result = rect });
            resultSpy.wait();
            return result;
        }

        function doubleTapAtPoint(x, y) {
            scaleSpy.clear()
            test.touchDoubleTap(webView, x, y)
            scaleSpy.wait()
        }

        function test_basic() {
            webView.url = webView.content
            verify(webView.waitForLoadSucceeded())

            compare("480x720", documentSize())

            compare(1.0, test.contentsScale)

            var rect = elementRect("target");
            var newScale = webView.width / (rect.width + 2 * 10) // inflated by 10px
            doubleTapAtPoint(rect.left + rect.height / 2, rect.top + rect.width / 2)

            compare(newScale, test.contentsScale)
        }
    }
}
