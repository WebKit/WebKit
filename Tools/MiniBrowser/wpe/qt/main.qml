import QtQuick 2.11
import QtQuick.Window 2.11
import org.wpewebkit.qtwpe 1.0

Window {
    id: main_window
    visible: true
    width: 1280
    height: 720
    title: qsTr("Hello WPE!")

    WPEView {
        url: initialUrl
        focus: true
        anchors.fill: parent
        onTitleChanged: {
            main_window.title = title;
        }
    }
}
