description("Test change of unit type for SVGLength animation.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "0");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "width");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "100");
animate.setAttribute("to", "200px");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.width.animVal.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");
    shouldBe("rect.width.baseVal.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");
}

function sample2() {
    shouldBe("rect.width.animVal.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");
    shouldBe("rect.width.baseVal.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");
}

function sample3() {
    shouldBe("rect.width.animVal.unitType", "SVGLength.SVG_LENGTHTYPE_PX");
    shouldBe("rect.width.baseVal.unitType", "SVGLength.SVG_LENGTHTYPE_NUMBER");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.5,   sample2],
        ["animation", 2.5,   sample3],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
