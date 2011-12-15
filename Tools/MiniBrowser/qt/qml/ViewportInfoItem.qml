import QtQuick 2.0
import QtWebKit 3.0

Rectangle {
    property var viewportInfo

    color: "black"
    opacity: 0.8

    Text {
        id: viewportInfoLabel
        text: "Viewport Info"
        color: "white"
    }
    Text {
        id: currentScaleLabel
        anchors.top: viewportInfoLabel.bottom
        text: "Current scale: " + viewportInfo.currentScale
        color: "white"
    }
    Text {
        id: initialScaleLabel
        anchors.top: currentScaleLabel.bottom
        text: "Initial scale: " + viewportInfo.initialScale
        color: "white"
    }
    Text {
        id: minimumScaleLabel
        anchors.top: initialScaleLabel.bottom
        text: "Minimum scale: " + viewportInfo.minimumScale
        color: "white"
    }
    Text {
        id: maximumScaleLabel
        anchors.top: minimumScaleLabel.bottom
        text: "Maximum scale: " + viewportInfo.maximumScale
        color: "white"
    }
    Text {
        id: devicePixelRatioLabel
        anchors.top: maximumScaleLabel.bottom
        text: "Device pixel ration: " + viewportInfo.devicePixelRatio
        color: "white"
    }
    Text {
        id: contentsSizeLabel
        anchors.top: devicePixelRatioLabel.bottom
        text: "Contents size: (" + viewportInfo.contentsSize.width + "x" + viewportInfo.contentsSize.height + ")"
        color: "white"
    }
    Text {
        id: layoutSizeLabel
        anchors.top: contentsSizeLabel.bottom
        text: "Viewport layout size: (" + viewportInfo.layoutSize.width + "x" + viewportInfo.layoutSize.height + ")"
        color: "white"
    }
    Text {
        id: scalableLabel
        anchors.top: layoutSizeLabel.bottom
        text: "View " + (viewportInfo.isScalable ? "is " : "is not " ) + "scalable."
        color: "white"
    }
}
