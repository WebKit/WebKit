description("Tests animation on 'currentColor'.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100px");
rect.setAttribute("height", "100px");
rect.setAttribute("fill", "currentColor");
rect.setAttribute("color", "red");
rect.setAttribute("onclick", "executeTest()");

var animateColor = createSVGElement("animateColor");
animateColor.setAttribute("id", "animateColor");
animateColor.setAttribute("attributeName", "color");
animateColor.setAttribute("from", "red");
animateColor.setAttribute("to", "green");
animateColor.setAttribute("dur", "3s");
animateColor.setAttribute("begin", "click");
animateColor.setAttribute("fill", "freeze");
rect.appendChild(animateColor);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    debug("");
    debug("Initial condition:");

    expectFillColor(rect, 255, 0, 0);
    shouldBeEqualToString("fillPaint.uri", "");
    shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_CURRENTCOLOR");
    shouldBe("fillPaint.colorType", "SVGColor.SVG_COLORTYPE_CURRENTCOLOR");
}

function sample2() {
    debug("");
    debug("Half-time condition:");

    expectFillColor(rect, 128, 64, 0);
    shouldBeEqualToString("fillPaint.uri", "");
    shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_CURRENTCOLOR");
    shouldBe("fillPaint.colorType", "SVGColor.SVG_COLORTYPE_CURRENTCOLOR");
}

function sample3() {
    debug("");
    debug("End condition:");

    expectFillColor(rect, 0, 128, 0);
    shouldBeEqualToString("fillPaint.uri", "");
    shouldBe("fillPaint.paintType", "SVGPaint.SVG_PAINTTYPE_CURRENTCOLOR");
    shouldBe("fillPaint.colorType", "SVGColor.SVG_COLORTYPE_CURRENTCOLOR");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animateColor", 0.0, sample1],
        ["animateColor", 1.5, sample2],
        ["animateColor", 3.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
