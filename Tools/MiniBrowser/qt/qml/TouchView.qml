import QtQuick 2.0
import QtWebKit 3.0

TouchWebView {
    id: webView
    property variant navigation: webView.page.navigation
    property string url: webView.page.url
    property int loadProgress: webView.page.loadProgress
    property string title: webView.page.title
    signal navigationStateChanged

    function load(address) {
        page.load(address)
    }

    Component.onCompleted: page.navigation.navigationStateChanged.connect(navigationStateChanged)
}
