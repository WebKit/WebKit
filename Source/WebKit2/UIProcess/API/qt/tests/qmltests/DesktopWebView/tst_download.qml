import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

DesktopWebView {
    id: webView
    width: 200
    height: 400

    property int expectedLength: 0
    property bool downloadFinished: false
    property int totalBytes: 0

    SignalSpy {
        id: loadSpy
        target: webView
        signalName: "loadSucceeded"
    }

    SignalSpy {
        id: spy
        target: webView
        signalName: "downloadRequested"
    }

    onDownloadRequested: {
        download.target = downloadItem
        expectedLength = downloadItem.expectedContentLength
        downloadItem.destinationPath = downloadItem.suggestedFilename
        downloadItem.start()
    }

    Connections {
        id: download
        ignoreUnknownSignals: true
        onSucceeded: {
            downloadFinished = true
            totalBytes = download.target.totalBytesReceived
        }
    }

    TestCase {
        name: "DesktopWebViewDownload"

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
            loadSpy.clear()
            expectedLength = 0
            downloadFinished = false
            totalBytes = 0
        }

        function test_downloadRequest() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.html"))
            loadSpy.wait()
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
        }

        function test_expectedLength() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.html"))
            loadSpy.wait()
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
            compare(expectedLength, 325)
        }

        function test_succeeded() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.html"))
            loadSpy.wait()
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
            verify(downloadFinished)
            compare(totalBytes, expectedLength)
        }
    }
}
