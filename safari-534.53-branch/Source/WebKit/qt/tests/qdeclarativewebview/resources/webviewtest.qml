import Qt 4.7
import QtWebKit 1.0

Flickable {
    id: flick
    width: 640
    height: 400
    clip: true
    contentWidth: myweb.width; contentHeight: myweb.height
    property alias myurl: myweb.url
    property alias prefHeight: myweb.preferredHeight
    property alias prefWidth: myweb.preferredWidth
    property url testUrl;
    WebView {
        id: myweb
        url: testUrl
        smooth: false
        scale: 1.0
        preferredHeight: 500
        preferredWidth: 600
        pressGrabTime: 1000
        focus: true
    }
}
