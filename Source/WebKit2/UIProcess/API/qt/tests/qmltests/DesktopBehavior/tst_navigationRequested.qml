import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

Item {
    property int expectedLength: 0
    property int totalBytes: 0
    property bool shouldDownload: false

    DesktopWebView {
        id: webView
        width: 200
        height: 200

        signal downloadFinished()

        onNavigationRequested: {
            if (shouldDownload)
                request.action = WebView.DownloadRequest
            else if (request.button == Qt.MiddleButton && request.modifiers & Qt.ControlModifier) {
                otherWebView.load(request.url)
                request.action = WebView.IgnoreRequest
            }
        }

        experimental.onDownloadRequested: {
            download.target = downloadItem
            expectedLength = downloadItem.expectedContentLength
            downloadItem.destinationPath = downloadItem.suggestedFilename
            downloadItem.start()
        }

        Connections {
            id: download
            ignoreUnknownSignals: true
            onSucceeded: {
                totalBytes = download.target.totalBytesReceived
                webView.downloadFinished()
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

    SignalSpy {
        id: downloadSpy
        target: webView.experimental
        signalName: "downloadRequested"
    }

    SignalSpy {
        id: downloadFinishedSpy
        target: webView
        signalName: "downloadFinished"
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

        function init() {
            spy.clear()
            otherSpy.clear()
            downloadSpy.clear()
            downloadFinishedSpy.clear()
            shouldDownload = false
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

        function test_downloadPolicy() {
            webView.load(Qt.resolvedUrl("../common/test2.html"))
            spy.wait()
            spy.clear()
            compare(spy.count, 0)
            downloadSpy.clear()
            downloadFinishedSpy.clear()
            expectedLength = 0
            shouldDownload = true
            mouseClick(webView, 100, 100, Qt.LeftButton)
            downloadSpy.wait()
            compare(downloadSpy.count, 1)
            downloadFinishedSpy.wait()
            compare(downloadFinishedSpy.count, 1)
            compare(totalBytes, expectedLength)
        }
    }
}
