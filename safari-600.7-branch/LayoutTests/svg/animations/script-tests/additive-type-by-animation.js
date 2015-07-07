description("This by animation for all XML property types");
embedSVGTestCase("resources/additive-type-by-animation.svg");

// Setup animation test
function checkBaseVal() {
    shouldBe("marker.orientAngle.baseVal.value", "-45");
    shouldBe("feConvolveMatrix.divisor.baseVal", "37.5");
    shouldBe("feConvolveMatrix.orderX.baseVal", "6");
    shouldBe("feConvolveMatrix.orderY.baseVal", "6");
    shouldBe("feConvolveMatrix.targetX.baseVal", "5");
    shouldBe("feConvolveMatrix.kernelUnitLengthX.baseVal", "20");
    shouldBe("feConvolveMatrix.kernelUnitLengthY.baseVal", "30");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.numberOfItems", "9");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(0).value", "0");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(1).value", "1");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(2).value", "0");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(3).value", "0");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(4).value", "1");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(5).value", "0");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(6).value", "0");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(7).value", "1");
    shouldBe("feConvolveMatrix.kernelMatrix.baseVal.getItem(8).value", "0");
    shouldBe("rect.y.baseVal.value", "0");
    shouldBe("text.dy.baseVal.numberOfItems", "4");
    shouldBe("text.dy.baseVal.getItem(0).value", "5");
    shouldBe("text.dy.baseVal.getItem(1).value", "-10");
    shouldBe("text.dy.baseVal.getItem(2).value", "10");
    shouldBe("text.dy.baseVal.getItem(3).value", "-10");
    shouldBe("svg.viewBox.baseVal.x", "0");
    shouldBe("svg.viewBox.baseVal.y", "0");
    shouldBe("svg.viewBox.baseVal.width", "300");
    shouldBe("svg.viewBox.baseVal.height", "300");
    shouldBe("polyline.points.numberOfItems", "4");
    shouldBe("polyline.points.getItem(0).x", "0");
    shouldBe("polyline.points.getItem(0).y", "0");
    shouldBe("polyline.points.getItem(1).x", "10");
    shouldBe("polyline.points.getItem(1).y", "5");
    shouldBe("polyline.points.getItem(1).x", "10");
    shouldBe("polyline.points.getItem(1).y", "5");
    shouldBe("polyline.points.getItem(2).x", "0");
    shouldBe("polyline.points.getItem(2).y", "10");
    shouldBe("path.transform.baseVal.numberOfItems", "1");
    shouldBe("path.transform.baseVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBe("path.transform.baseVal.getItem(0).angle", "45");
}

function sample1() {
    shouldBeCloseEnough("marker.orientAngle.animVal.value", "-45");
    shouldBeCloseEnough("feConvolveMatrix.divisor.animVal", "37.5");
    shouldBeCloseEnough("feConvolveMatrix.orderX.animVal", "6");
    shouldBeCloseEnough("feConvolveMatrix.orderY.animVal", "6");
    shouldBeCloseEnough("feConvolveMatrix.targetX.animVal", "5");
    shouldBeCloseEnough("feConvolveMatrix.kernelUnitLengthX.animVal", "20");
    shouldBeCloseEnough("feConvolveMatrix.kernelUnitLengthY.animVal", "30");
    shouldBe("feConvolveMatrix.kernelMatrix.animVal.numberOfItems", "9");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(0).value", "0");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(1).value", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(2).value", "0");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(3).value", "0");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(4).value", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(5).value", "0");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(6).value", "0");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(7).value", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(8).value", "0");
    shouldBeCloseEnough("rect.y.animVal.value", "0");
    shouldBe("text.dy.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.dy.animVal.getItem(0).value", "5");
    shouldBeCloseEnough("text.dy.animVal.getItem(1).value", "-10");
    shouldBeCloseEnough("text.dy.animVal.getItem(2).value", "10");
    shouldBeCloseEnough("text.dy.animVal.getItem(3).value", "-10");
    shouldBeCloseEnough("svg.viewBox.animVal.x", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.y", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.width", "300");
    shouldBeCloseEnough("svg.viewBox.animVal.height", "300");
    shouldBe("polyline.animatedPoints.numberOfItems", "4");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(0).x", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(0).y", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).x", "10");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).y", "5");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).x", "10");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).y", "5");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(2).x", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(2).y", "10");
    shouldBe("path.transform.animVal.numberOfItems", "2");
    shouldBe("path.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBe("path.transform.animVal.getItem(0).angle", "45");
    shouldBe("path.transform.animVal.getItem(1).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBeCloseEnough("path.transform.animVal.getItem(1).angle", "0");
    expectFillColor(rect, 0, 0, 0);
    checkBaseVal();
}

function sample2() {
    shouldBeCloseEnough("marker.orientAngle.animVal.value", "-22.5");
    shouldBeCloseEnough("feConvolveMatrix.divisor.animVal", "28.75");
    shouldBeCloseEnough("feConvolveMatrix.orderX.animVal", "5");
    shouldBeCloseEnough("feConvolveMatrix.orderY.animVal", "5");
    shouldBeCloseEnough("feConvolveMatrix.targetX.animVal", "3");
    shouldBeCloseEnough("feConvolveMatrix.kernelUnitLengthX.animVal", "15");
    shouldBeCloseEnough("feConvolveMatrix.kernelUnitLengthY.animVal", "20");
    shouldBe("feConvolveMatrix.kernelMatrix.animVal.numberOfItems", "9");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(0).value", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(1).value", "1.5");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(2).value", "1.5");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(3).value", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(4).value", "1.5");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(5).value", "1.5");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(6).value", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(7).value", "1.5");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(8).value", "1.5");
    shouldBeCloseEnough("rect.y.animVal.value", "50");
    shouldBe("text.dy.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.dy.animVal.getItem(0).value", "0");
    shouldBeCloseEnough("text.dy.animVal.getItem(1).value", "0");
    shouldBeCloseEnough("text.dy.animVal.getItem(2).value", "0");
    shouldBeCloseEnough("text.dy.animVal.getItem(3).value", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.x", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.y", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.width", "250");
    shouldBeCloseEnough("svg.viewBox.animVal.height", "250");
    shouldBe("polyline.animatedPoints.numberOfItems", "4");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(0).x", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(0).y", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).x", "15");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).y", "7.5");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).x", "15");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).y", "7.5");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(2).x", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(2).y", "15");
    shouldBe("path.transform.animVal.numberOfItems", "2");
    shouldBe("path.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBe("path.transform.animVal.getItem(0).angle", "45");
    shouldBe("path.transform.animVal.getItem(1).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBeCloseEnough("path.transform.animVal.getItem(1).angle", "-22.5");
    expectFillColor(rect, 0, 63, 0);
    checkBaseVal();
}

function sample3() {
    shouldBeCloseEnough("marker.orientAngle.animVal.value", "0");
    shouldBeCloseEnough("feConvolveMatrix.divisor.animVal", "20");
    shouldBeCloseEnough("feConvolveMatrix.orderX.animVal", "3");
    shouldBeCloseEnough("feConvolveMatrix.orderY.animVal", "3");
    shouldBeCloseEnough("feConvolveMatrix.targetX.animVal", "1");
    shouldBeCloseEnough("feConvolveMatrix.kernelUnitLengthX.animVal", "10");
    shouldBeCloseEnough("feConvolveMatrix.kernelUnitLengthY.animVal", "10");
    shouldBe("feConvolveMatrix.kernelMatrix.animVal.numberOfItems", "9");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(0).value", "2");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(1).value", "2");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(2).value", "3");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(3).value", "2");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(4).value", "2");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(5).value", "3");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(6).value", "2");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(7).value", "2");
    shouldBeCloseEnough("feConvolveMatrix.kernelMatrix.animVal.getItem(8).value", "3");
    shouldBeCloseEnough("rect.y.animVal.value", "100");
    shouldBe("text.dy.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.dy.animVal.getItem(0).value", "-5");
    shouldBeCloseEnough("text.dy.animVal.getItem(1).value", "10");
    shouldBeCloseEnough("text.dy.animVal.getItem(2).value", "-10");
    shouldBeCloseEnough("text.dy.animVal.getItem(3).value", "10");
    shouldBeCloseEnough("svg.viewBox.animVal.x", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.y", "0");
    shouldBeCloseEnough("svg.viewBox.animVal.width", "200");
    shouldBeCloseEnough("svg.viewBox.animVal.height", "200");
    shouldBe("polyline.animatedPoints.numberOfItems", "4");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(0).x", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(0).y", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).x", "20");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).y", "10");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).x", "20");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(1).y", "10");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(2).x", "0");
    shouldBeCloseEnough("polyline.animatedPoints.getItem(2).y", "20");
    shouldBe("path.transform.animVal.numberOfItems", "2");
    shouldBe("path.transform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBe("path.transform.animVal.getItem(0).angle", "45");
    shouldBe("path.transform.animVal.getItem(1).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBeCloseEnough("path.transform.animVal.getItem(1).angle", "-45");
    expectFillColor(rect, 0, 128, 0);
    checkBaseVal();
}

function executeTest() {
    marker = rootSVGElement.ownerDocument.getElementsByTagName("marker")[0];
    filter = rootSVGElement.ownerDocument.getElementsByTagName("filter")[0];
    feConvolveMatrix = rootSVGElement.ownerDocument.getElementsByTagName("feConvolveMatrix")[0];
    rect = rootSVGElement.ownerDocument.getElementsByTagName("rect")[0];
    svg = rootSVGElement.ownerDocument.getElementsByTagName("svg")[0];
    path = rootSVGElement.ownerDocument.getElementsByTagName("path")[0];
    polyline = rootSVGElement.ownerDocument.getElementsByTagName("polyline")[0];
    text = rootSVGElement.ownerDocument.getElementsByTagName("text")[0];

    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["an1", 0.0,   sample1],
        ["an1", 2.0,   sample2],
        ["an1", 4.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

window.animationStartsImmediately = true;
var successfullyParsed = true;
