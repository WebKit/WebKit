import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import "../common"

TestWebView {
    id: webView
    property variant lastUrl
    property bool watchProgress: false
    property int numLoadStarted: 0
    property int numLoadSucceeded: 0

    onLoadProgressChanged: {
        if (watchProgress && webView.loadProgress != 100) {
            watchProgress = false
            url = ''
        }
    }

    onLoadingChanged: {
        if (loadRequest.status == WebView.LoadStartedStatus)
            ++numLoadStarted
        if (loadRequest.status == WebView.LoadSucceededStatus)
            ++numLoadSucceeded
    }

    TestCase {
        id: test
        name: "WebViewLoadUrl"
        function test_loadIgnoreEmptyUrl() {
            var url = Qt.resolvedUrl("../common/test1.html")

            webView.url = url
            verify(webView.waitForLoadSucceeded())
            compare(numLoadStarted, 1)
            compare(numLoadSucceeded, 1)
            compare(webView.url, url)

            lastUrl = webView.url
            webView.url = ''
            wait(1000)
            compare(numLoadStarted, 1)
            compare(numLoadSucceeded, 1)
            compare(webView.url, lastUrl)

            webView.url = 'about:blank'
            verify(webView.waitForLoadSucceeded())
            compare(numLoadStarted, 2)
            compare(numLoadSucceeded, 2)
            compare(webView.url, 'about:blank')

            // It shouldn't interrupt any ongoing load when an empty url is used.
            watchProgress = true
            webView.url = url
            webView.waitForLoadSucceeded()
            compare(numLoadStarted, 3)
            compare(numLoadSucceeded, 3)
            verify(!watchProgress)
            compare(webView.url, url)
        }
    }
}
