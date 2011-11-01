import QtQuick 2.0
import QtWebKit 3.0

TouchWebView {
    signal navigationStateChanged

    Component.onCompleted: navigation.navigationStateChanged.connect(navigationStateChanged)
}
