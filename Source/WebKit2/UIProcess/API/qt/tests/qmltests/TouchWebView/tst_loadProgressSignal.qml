import QtQuick 2.0
import QtTest 1.0
import QtWebKit.experimental 5.0

TouchWebView {
    id: webView

    SignalSpy {
        id: spy
        target: webView.page
        signalName: "loadSucceeded"
    }

    SignalSpy {
        id: spyProgress
        target: webView.page
        signalName: "loadProgressChanged"
    }

    TestCase {
        name: "TouchWebViewLoadProgressSignal"

        function test_loadProgressSignal() {
            compare(spyProgress.count, 0)
            compare(webView.page.loadProgress, 0)
            webView.page.load(Qt.resolvedUrl("../common/test1.html"))
            spyProgress.wait()
            compare(true, webView.page.loadProgress > -1 && webView.page.loadProgress < 101)
            if (webView.page.loadProgress > 0 && webView.page.loadProgress < 100) {
                spy.wait()
                spyProgress.wait()
                compare(webView.page.loadProgress, 100)
            }
        }
    }
}
