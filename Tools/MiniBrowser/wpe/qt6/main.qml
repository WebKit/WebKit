import QtQuick
import QtQuick.Controls 2.11
import QtQuick.Controls.Material 2.11
import QtQuick.Layouts 2.11
import QtQuick.Window 2.11

import org.wpewebkit.qtwpe 1.0

Window {
    id: main_window
    visible: true
    width: 1280
    height: 720
    title: qsTr("qt-wpe-minibrowser")

    ColumnLayout {
        id: main_layout
        anchors.fill: parent

        RowLayout {
            id: header_layout

            Button {
                id: back_button
                enabled: false
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                text: qsTr("<")
                font.pointSize: 20

                background: Rectangle {
                    opacity: enabled ? 1.0 : 0.1
                }

                onClicked: function() { web_view.goBack(); }
            }

            Button {
                id: forward_button
                enabled: false
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                font.pointSize: 20
                text: qsTr(">")

                background: Rectangle {
                    opacity: enabled ? 1.0 : 0.1
                }

                onClicked: function() { web_view.goForward(); }
            }

            Button {
                id: reload_button
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                font.pointSize: 20
                text: qsTr("↻")

                onClicked: function() { web_view.reload(); }

            }

            TextField {
                id: url_bar
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                font.pointSize: 13
                text: initialUrl

                onAccepted: {
                    url_bar.text = urlHelper.parseUserUrl(url_bar.text)
                    web_view.url = url_bar.text
                }
            }

            Button {
                id: confirm_button
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                font.pointSize: 20
                text: qsTr("↪")
                onClicked: function() {
                    url_bar.text = urlHelper.parseUserUrl(url_bar.text)
                    web_view.url = url_bar.text
                }
            }

            Button {
                id: close_button
                Layout.preferredWidth: 50
                Layout.preferredHeight: 50
                font.pointSize: 20
                text: qsTr("x")

                onClicked: function() { main_window.close(); }
            }
        }

        ProgressBar {
            id: progress_bar
            Layout.fillWidth: true
        }

        RowLayout {
            id: web_layout

            WPEView {
                id: web_view
                Layout.fillWidth: true
                Layout.fillHeight: true
                url: initialUrl

                onTitleChanged: function() {
                    main_window.title = title;
                }

                onLoadingChanged: function(loadRequest) {
                    back_button.enabled = web_view.canGoBack;
                    forward_button.enabled = web_view.canGoForward;

                    if (loadRequest.errorString)
                        console.log('WPEView error: ' + loadRequest.errorString);
                }

                onUrlChanged: function() {
                    url_bar.text = web_view.url;
                }

                onLoadProgressChanged: function() {
                    var value = web_view.loadProgress / 100;
                    progress_bar.value = value;
                    progress_bar.visible = value == 1 ? false : true;
                }
            }
        }
    }
}
