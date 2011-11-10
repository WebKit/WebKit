/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import QtQuick 2.0
import QtWebKit 3.0

Rectangle {
    // Do not define anchors or an initial size here! This would mess up with QSGView::SizeRootObjectToView.

    property bool canGoBack: false
    property bool canGoForward: false
    property bool canStop: false
    property alias webview: webView

    signal pageTitleChanged(string title)

    function load(address) {
        webView.load(address)
    }

    function focusAddressBar() {
        addressLine.forceActiveFocus()
        addressLine.selectAll()
    }

    Rectangle {
        id: navigationBar
        color: "#efefef"
        height: 30
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Row {
            id: controlsRow
            spacing: 4
            Rectangle {
                id: backButton
                height: navigationBar.height - 2
                width: height
                color: "#efefef"

                Image {
                    anchors.centerIn: parent
                    source: "../icons/previous.png"
                }

                Rectangle {
                    anchors.fill: parent
                    color: reloadButton.color
                    opacity: 0.8
                    visible: !canGoBack
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("going back")
                        webView.navigation.goBack()
                    }
                }
            }
            Rectangle {
                id: forwardButton
                height: navigationBar.height - 2
                width: height
                color: "#efefef"

                Image {
                    anchors.centerIn: parent
                    source: "../icons/next.png"
                }

                Rectangle {
                    anchors.fill: parent
                    color: forwardButton.color
                    opacity: 0.8
                    visible: !canGoForward
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("going forward")
                        webView.navigation.goForward()
                    }
                }
            }
            Rectangle {
                id: reloadButton
                height: navigationBar.height - 2
                width: height
                color: "#efefef"

                Image {
                    anchors.centerIn: parent
                    source: canStop ? "../icons/stop.png" : "../icons/refresh.png"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (canStop) {
                            console.log("stop loading")
                            webView.navigation.stop()
                        } else {
                            console.log("reloading")
                            webView.navigation.reload()
                        }
                    }
                }
            }
        }
        Rectangle {
            color: "white"
            height: navigationBar.height - 4
            border.width: 1
            anchors {
                left: controlsRow.right
                right: parent.right
                margins: 2
                verticalCenter: parent.verticalCenter
            }
            Rectangle {
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                }
                width: parent.width / 100 * webView.loadProgress
                color: "blue"
                opacity: 0.3
                visible: webView.loadProgress != 100
            }

            TextInput {
                id: addressLine
                clip: true
                selectByMouse: true
                font {
                    pointSize: 14
                    family: "Helvetica"
                }
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    right: parent.right
                    margins: 2
                }

                Keys.onReturnPressed:{
                    console.log("going to: ", addressLine.text)
                    webView.load(utils.urlFromUserInput(addressLine.text))
                }

                Keys.onPressed: {
                    if (((event.modifiers & Qt.ControlModifier) && event.key == Qt.Key_L) || event.key == Qt.key_F6) {
                        focusAddressBar()
                        event.accepted = true
                    }
                }
            }
        }
    }

    WebView {
        id: webView
        anchors {
            top: navigationBar.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        signal navigationStateChanged

        Component.onCompleted: navigation.navigationStateChanged.connect(navigationStateChanged);

        onTitleChanged: pageTitleChanged(title)
        onUrlChanged: {
            addressLine.text = url
            if (options.printLoadedUrls)
                console.log("Loaded:", webView.url);
            forceActiveFocus();
        }
        onNavigationStateChanged: {
            canGoBack = navigation.canGoBack
            canGoForward = navigation.canGoForward
            canStop = navigation.canStop
        }
    }

    Keys.onPressed: {
        if (((event.modifiers & Qt.ControlModifier) && event.key == Qt.Key_L) || event.key == Qt.key_F6) {
            focusAddressBar()
            event.accepted = true
        }
    }
}
