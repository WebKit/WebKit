import QtQuick 2.0
import QtWebKit 3.0

Rectangle {
    property var test

    color: "black"
    opacity: 0.8

    Item {
        anchors.fill: parent
        anchors.margins: 20

        property string fontFamily: "Nokia Pure, Helvetica"
        property color fontColor: "white"

        Text {
            id: viewportInfoLabel
            text: "Viewport Info"
            color: "white"
            font.family: "Nokia Pure, Helvetica"
            font.pointSize: 24
        }
        Text {
            id: currentScaleLabel
            anchors.top: viewportInfoLabel.bottom
            anchors.topMargin: 30
            text: "Current scale: " + parseFloat(test.contentsScale.toFixed(4))
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: initialScaleLabel
            anchors.top: currentScaleLabel.bottom
            text: "Initial scale: " + parseFloat(test.initialScale.toFixed(4))
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: minimumScaleLabel
            anchors.top: initialScaleLabel.bottom
            text: "Minimum scale: " + parseFloat(test.minimumScale.toFixed(4))
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: maximumScaleLabel
            anchors.top: minimumScaleLabel.bottom
            text: "Maximum scale: " + parseFloat(test.maximumScale.toFixed(4))
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: devicePixelRatioLabel
            anchors.top: maximumScaleLabel.bottom
            anchors.topMargin: 30
            text: "Device pixel ratio: " + parseFloat(test.devicePixelRatio.toFixed(4))
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: contentsSizeLabel
            anchors.top: devicePixelRatioLabel.bottom
            text: "Contents size: " + test.contentsSize.width + "x" + test.contentsSize.height
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: layoutSizeLabel
            anchors.top: contentsSizeLabel.bottom
            text: "Viewport layout size: " + test.layoutSize.width + "x" + test.layoutSize.height
            font.family: parent.fontFamily
            color: parent.fontColor
        }
        Text {
            id: scalableLabel
            anchors.top: layoutSizeLabel.bottom
            anchors.topMargin: 30
            text: "View " + (test.isScalable ? "is " : "is not " ) + "scalable."
            font.family: parent.fontFamily
            color: parent.fontColor
        }
    }
}
