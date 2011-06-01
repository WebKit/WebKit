// Demonstrates opening new WebViews from HTML

import QtQuick 1.0
import QtWebKit 1.0

Grid {
    columns: 3
    id: pages
    height: 300; width: 600
    property int pagesOpened: 0
    property Item firstPageOpened: null

    Component {
        id: webViewPage
        Rectangle {
            id: thisPage
            width: 150
            height: 150
            property WebView webView: wv

            WebView {
                id: wv
                anchors.fill: parent
                newWindowComponent: webViewPage
                newWindowParent: pages
                url: "newwindows.html"
                Component.onCompleted: {
                    if (pagesOpened == 1) {
                        pages.firstPageOpened = thisPage;
                    }
                }
            }
        }
    }

    Loader {
        id: originalPage
        sourceComponent: webViewPage
        property bool ready: status == Loader.Ready && item.webView.status == WebView.Ready
    }

    Timer {
        interval: 10
        running: originalPage.ready && pagesOpened < 4
        repeat: true
        onTriggered: {
            pagesOpened++;
            originalPage.item.webView.evaluateJavaScript("clickTheLink()");
        }
    }
}
