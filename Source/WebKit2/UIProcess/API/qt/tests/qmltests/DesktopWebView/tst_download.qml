import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0

DesktopWebView {
    id: webView
    width: 200
    height: 400

    property int expectedLength : 0
    property int totalBytes : 0

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
        onSucceeded: { totalBytes = download.target.totalBytesReceived }
    }

    SignalSpy {
        id: otherSpy
        target: download
        signalName: "succeeded"
    }

    TestCase {
        name: "DesktopWebViewDownload"
        when: windowShown

        function test_downloadRequest() {
            spy.clear()
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.html"))
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
        }

        function test_expectedLength() {
            spy.clear()
            compare(spy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.html"))
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
            compare(expectedLength, 325)
        }

        function test_succeeded() {
            spy.clear()
            compare(spy.count, 0)
            otherSpy.clear()
            compare(otherSpy.count, 0)
            webView.load(Qt.resolvedUrl("../common/download.html"))
            mouseClick(webView, 100, 100, Qt.LeftButton)
            spy.wait()
            compare(spy.count, 1)
            otherSpy.wait()
            compare(otherSpy.count, 1)
            compare(totalBytes, expectedLength)
        }
    }
}
