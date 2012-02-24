import QtQuick 2.0
import QtTest 1.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0

WebView {
    property var loadStatus: null

    function waitForLoadSucceeded() {
        var timeout = 5000
        var i = 0
        while (i < timeout && loadStatus != WebView.LoadSucceededStatus) {
            testResult.wait(50)
            i += 50
        }
        var success = loadStatus == WebView.LoadSucceededStatus
        loadStatus = null
        return success
    }
    function waitForLoadFailed() {
        var timeout = 5000
        var i = 0
        while (i < timeout && loadStatus != WebView.LoadFailedStatus) {
            testResult.wait(50)
            i += 50
        }
        var failure = loadStatus == WebView.LoadFailedStatus
        loadStatus = null
        return failure
    }

    TestResult { id: testResult }

    onLoadingChanged: loadStatus = loadRequest.status
}

