import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 3.0

WebView {
    id: webView
    width: 200
    height: 200

    property int expectedLength: 0
    property bool downloadFinished: false
    property int totalBytes: 0

    SignalSpy {
        id: spy
        target: experimental
        signalName: "downloadRequested"
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
            downloadFinished = true
            totalBytes = download.target.totalBytesReceived
        }
    }

    TestCase {
        name: "WebViewDownload"

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
            expectedLength = 0
            downloadFinished = false
        }

        function test_downloadRequest() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.zip"))
            spy.wait()
            compare(spy.count, 1)
        }

        function test_expectedLength() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.zip"))
            spy.wait()
            compare(spy.count, 1)
            compare(expectedLength, 325)
        }

        function test_succeeded() {
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.zip"))
            spy.wait()
            compare(spy.count, 1)
            verify(downloadFinished)
            compare(totalBytes, expectedLength)
        }
    }
}
