import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

WebView {
    id: webView
    property variant lastUrl
    property bool watchProgress: false

    onLoadProgressChanged: {
        if (watchProgress && webView.loadProgress != 100) {
            watchProgress = false
            load('')
        }
    }

    SignalSpy {
        id: spy
        target: webView
        signalName: "loadSucceeded"
    }

    TestCase {
        id: test
        name: "WebViewLoadUrl"
        function test_loadIgnoreEmptyUrl() {
            compare(spy.count, 0)
            var url = Qt.resolvedUrl("../common/test1.html")

            webView.load(url)
            spy.wait()
            compare(spy.count, 1)
            compare(webView.url, url)

            lastUrl = webView.url
            webView.load('')
            wait(1000)
            compare(spy.count, 1)
            compare(webView.url, lastUrl)

            webView.load('about:blank')
            spy.wait()
            compare(spy.count, 2)
            compare(webView.url, 'about:blank')

            // It shouldn't interrupt any ongoing load when an empty url is used.
            watchProgress = true
            webView.load(url)
            spy.wait()
            compare(spy.count, 3)
            verify(!watchProgress)
            compare(webView.url, url)
        }
    }
}
