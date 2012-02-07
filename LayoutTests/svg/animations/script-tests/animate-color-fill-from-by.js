description("Tests animation on 'currentColor'.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "currentColor");
rect.setAttribute("color", "d00000");

var animateColor = createSVGElement("animateColor");
animateColor.setAttribute("id", "animateColor");
animateColor.setAttribute("attributeName", "color");
animateColor.setAttribute("from", "#d00000");
animateColor.setAttribute("by", "#0000d0");
animateColor.setAttribute("dur", "3s");
animateColor.setAttribute("begin", "click");
animateColor.setAttribute("fill", "freeze");
rect.appendChild(animateColor);
rootSVGElement.appendChild(rect);

function checkFillColor(red, green, blue, hex) {
    shouldBeEqualToString("document.defaultView.getComputedStyle(rect).getPropertyValue('fill')", hex);

    try {
        shouldBeEqualToString("(fillPaint = document.defaultView.getComputedStyle(rect).getPropertyCSSValue('fill')).toString()", "[object SVGPaint]");
        shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_CURRENTCOLOR");
        shouldBeEqualToString("fillPaint.uri", "");
        shouldBe("fillPaint.colorType", "SVGColor.SVG_COLORTYPE_CURRENTCOLOR");
        shouldBeEqualToString("(fillColor = fillPaint.rgbColor).toString()", "[object RGBColor]");
        shouldBe("fillColor.red.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + red);
        shouldBe("fillColor.green.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + green);
        shouldBe("fillColor.blue.getFloatValue(CSSPrimitiveValue.CSS_NUMBER)", "" + blue);
    } catch(e) {
        // Opera doesn't support getPropertyCSSValue - no way to compare to their SVGPaint/SVGColor objects :(
    }
}

// Setup animation test
function sample1() {
    debug("");
    debug("Initial condition:");
    checkFillColor(208, 0, 0, "#d00000");
}

function sample2() {
    debug("");
    debug("Half-time condition:");
    checkFillColor(208, 0, 104, "#d00068");
}

function sample3() {
    debug("");
    debug("End condition:");
    checkFillColor(208, 0, 208, "#d000d0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animateColor", 0.0,    "rect", sample1],
        ["animateColor", 1.5,    "rect", sample2],
        ["animateColor", 3.0,    "rect", sample3]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
rect.setAttribute("onclick", "executeTest()");
window.setTimeout("triggerUpdate(50, 50)", 0);

var successfullyParsed = true;
