description("This test checks the SVGViewSpec API, operating on a parsed viewSpec");
if (window.testRunner)
    testRunner.waitUntilDone();

function completeTest() {
    var script = document.createElement("script");

    script.onload = function() {
        if (window.testRunner)
            testRunner.notifyDone();
    };

    script.src = "../../fast/js/resources/js-test-post.js";
    document.body.appendChild(script);
}

// Load an external file to test svgView() handling.
function testFragment(string) {
    debug("");
    debug("Loading external SVG resources/viewspec-target.svg");
    debug("Passing SVGViewSpec: " + string);
    debug("");
    var iframeElement = document.createElement("iframe");
    iframeElement.setAttribute("id", "iframe");
    iframeElement.setAttribute("style", "display: block");
    iframeElement.setAttribute("width", "120px");
    iframeElement.setAttribute("height", "120px");
    iframeElement.setAttribute("onload", "setTimeout(continueTesting, 0)");
    var newURL = "resources/viewspec-target.svg#" + string;
    iframeElement.src = newURL;

    document.getElementById("console").appendChild(iframeElement);
}

function matrixToString(matrix) {
    return "[" + matrix.a.toFixed(2) + " " + matrix.b.toFixed(2) + " " + matrix.c.toFixed(2) + " " + matrix.d.toFixed(2) + " " + matrix.e.toFixed(2) + " " + matrix.f.toFixed(2) + "]";
}

function continueTesting() {
    currentView = document.getElementById("iframe").contentDocument.documentElement.currentView;

    debug("");
    debug("Check transform value");
    shouldBeEqualToString("currentView.transformString", "translate(0 10) translate(25 25) rotate(45) translate(-25 -25) scale(0.7 0.7)");
    shouldBe("currentView.transform.numberOfItems", "5");

    shouldBe("currentView.transform.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBe("currentView.transform.getItem(0).angle", "0");
    shouldBeEqualToString("matrixToString(currentView.transform.getItem(0).matrix)", "[1.00 0.00 0.00 1.00 0.00 10.00]");

    shouldBe("currentView.transform.getItem(1).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBe("currentView.transform.getItem(1).angle", "0");
    shouldBeEqualToString("matrixToString(currentView.transform.getItem(1).matrix)", "[1.00 0.00 0.00 1.00 25.00 25.00]");

    shouldBe("currentView.transform.getItem(2).type", "SVGTransform.SVG_TRANSFORM_ROTATE");
    shouldBe("currentView.transform.getItem(2).angle", "45");
    shouldBeEqualToString("matrixToString(currentView.transform.getItem(2).matrix)", "[0.71 0.71 -0.71 0.71 0.00 0.00]");

    shouldBe("currentView.transform.getItem(3).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBe("currentView.transform.getItem(3).angle", "0");
    shouldBeEqualToString("matrixToString(currentView.transform.getItem(3).matrix)", "[1.00 0.00 0.00 1.00 -25.00 -25.00]");

    shouldBe("currentView.transform.getItem(4).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBe("currentView.transform.getItem(4).angle", "0");
    shouldBeEqualToString("matrixToString(currentView.transform.getItem(4).matrix)", "[0.70 0.00 0.00 0.70 0.00 0.00]");

    debug("");
    debug("Check viewTarget value");
    shouldBeEqualToString("currentView.viewTargetString", "blub");
    shouldBeNull("currentView.viewTarget"); // There's no element named 'blub' in the tree.

    debug("");
    debug("Check zoomAndPan value");
    shouldBe("currentView.zoomAndPan", "SVGZoomAndPan.SVG_ZOOMANDPAN_DISABLE");

    debug("");
    debug("Check viewBox value");
    shouldBe("currentView.viewBox.baseVal.x", "0");
    shouldBe("currentView.viewBox.baseVal.y", "0");
    shouldBe("currentView.viewBox.baseVal.width", "100");
    shouldBe("currentView.viewBox.baseVal.height", "50");
    shouldBeEqualToString("currentView.viewBoxString", "0 0 100 50");

    debug("");
    debug("Check preserveAspectRatio value");
    shouldBeEqualToString("currentView.preserveAspectRatioString", "xMinYMid slice");
    shouldBe("currentView.preserveAspectRatio.baseVal.align", "SVGPreserveAspectRatio.SVG_PRESERVEASPECTRATIO_XMINYMID");
    shouldBe("currentView.preserveAspectRatio.baseVal.meetOrSlice", "SVGPreserveAspectRatio.SVG_MEETORSLICE_SLICE");

    completeTest();
}

testFragment("svgView(viewBox(0,0,100,50);preserveAspectRatio(xMinYMid slice);transform(translate(0 10) translate(25 25) rotate(45) translate(-25 -25) scale(0.7 0.7));viewTarget(blub);zoomAndPan(disable))");
successfullyParsed = true;
