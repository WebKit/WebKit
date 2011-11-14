import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    SignalSpy {
        id: spyProgress
        target: webView
        signalName: "loadProgressChanged"
    }

    TestCase {
        name: "WebViewLoadProgressSignal"

        function test_loadProgressSignal() {
            compare(spyProgress.count, 0)
            compare(webView.loadProgress, 0)
            webView.load(Qt.resolvedUrl("../common/test1.html"))
            spyProgress.wait()
            compare(true, webView.loadProgress > -1 && webView.loadProgress < 101)
            if (webView.loadProgress > 0 && webView.loadProgress < 100) {
                spy.wait()
                spyProgress.wait()
                compare(webView.loadProgress, 100)
            }
        }
    }
}
