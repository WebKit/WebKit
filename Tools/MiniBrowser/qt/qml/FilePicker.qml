/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

import QtQuick 2.0
import Qt.labs.folderlistmodel 1.0

Rectangle {
    id: filePicker

    property QtObject fileModel: model
    property alias folder: folders.folder

    color: "white"
    width: 400
    height: 500

    smooth: true
    radius: 5
    anchors.centerIn: parent

    border {
        width: 1
        color: "#bfbfbf"
    }

    BorderImage {
        source: "../icons/titlebar.png";
        width: parent.width;
        height: 50
        y: -7
        id: titleBar

        anchors {
            top: parent.top
            bottom: folderListView.top
        }
        Rectangle {
            id: upButton
            width: 48
            height: titleBar.height - 7
            color: "transparent"
            Image { anchors.centerIn: parent; source: "../icons/up.png" }
            MouseArea { id: upRegion; anchors.centerIn: parent
                width: 48
                height: 48
                onClicked: if (folders.parentFolder != "") up()
            }
        }

        Rectangle {
            color: "gray"
            x: 48
            width: 1
            height: 44
        }

        Text {
            anchors {
                left: upButton.right
                right: parent.right
                leftMargin: 4
                rightMargin: 4
            }

            height: parent.height
            text: folders.folder
            color: "white"
            elide: Text.ElideLeft;
            horizontalAlignment: Text.AlignLeft;
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 24
        }
    }

    ListView {
        id: folderListView

        width: parent.width
        height: 400
        anchors.centerIn: parent
        spacing: 2
        clip: true

        FolderListModel {
            id: folders
        }

        Component {
            id: fileDelegate

            Rectangle {
                function selected() {
                    if (folders.isFolder(index))
                        openFolder(filePath);
                    else
                        fileModel.accept(filePath);
                }

                height: 50
                width: parent.width
                color: folders.isFolder(index) ? "lightgray": "darkgray"

                Item {
                    width: 48;
                    height: 48
                    Image {
                        source: "../icons/folder.png"
                        anchors.centerIn: parent
                        visible: folders.isFolder(index)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    anchors.leftMargin: 50
                    text: fileName
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: selected();
                }
            }
        }
        model: folders
        delegate: fileDelegate
    }

    Rectangle {
        id: button

        height: 50

        border {
            width: 1
            color: "#bfbfbf"
        }

        anchors {
            bottom: parent.bottom
            top: folderListView.bottom
            left: parent.left
            right: parent.right
        }

        DialogButton {
            id: cancel
            text: "Cancel"
            anchors {
                horizontalCenter: parent.horizontalCenter;
                verticalCenter:  parent.verticalCenter
            }

            onClicked: fileModel.reject()
        }
    }

    function openFolder(path) {
        folders.folder = path;
    }

    function up() {
        folders.folder = folders.parentFolder;
    }
}
