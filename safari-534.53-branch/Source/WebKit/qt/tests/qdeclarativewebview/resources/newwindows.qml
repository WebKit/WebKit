// Demonstrates opening new WebViews from HTML

import QtQuick 1.0
import QtWebKit 1.0

Grid {
    columns: 3
    id: pages
    height: 300; width: 600
    property int total: 0

    Component {
        id: webViewPage
        Rectangle {
            width: webView.width
            height: webView.height
            border.color: "gray"

            WebView {
                id: webView
                width: 150 // force predictable for test
                newWindowComponent: webViewPage
                newWindowParent: pages
                url: "newwindows.html"
                Timer {
                    interval: 10; running: total<4; repeat: false;
                    onTriggered: { if (webView.status==WebView.Ready) { total++; webView.evaluateJavaScript("clickTheLink()") } }
                }
            }
        }
    }

    Loader { sourceComponent: webViewPage }
}
