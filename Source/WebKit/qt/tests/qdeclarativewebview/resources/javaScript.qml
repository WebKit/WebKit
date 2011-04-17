import QtQuick 1.0
import QtWebKit 1.0

WebView {
    url: "javaScript.html"
    javaScriptWindowObjects: [
        QtObject {
            property string qmlprop: "qmlvalue"
            WebView.windowObjectName: "myjsname"
        }
    ]
}
