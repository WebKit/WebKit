import QtQuick 1.0
import QtWebKit 1.0

Item {
    width: 240
    height: 160
    Grid {
        anchors.fill: parent
        objectName: "newWindowParent"
        id: newWindowParent
    }

    Row {
        anchors.fill: parent
        id: oldWindowParent
        objectName: "oldWindowParent"
    }

    Loader {
        sourceComponent: webViewComponent
    }
    Component {
            id: webViewComponent
            WebView {
                id: webView
                objectName: "webView"
                newWindowComponent: webViewComponent
                newWindowParent: oldWindowParent         
                url: "basic.html"
                renderingEnabled: true
                pressGrabTime: 200
            }
    }
}
