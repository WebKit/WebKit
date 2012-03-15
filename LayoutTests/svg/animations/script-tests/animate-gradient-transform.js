description("Tests if gradientTransform of a gradient is animateable.");
createSVGTestCase();

// Setup test document
var gradient = createSVGElement("linearGradient");
gradient.setAttribute("id", "gradient");
gradient.setAttribute("gradientUnits", "userSpaceOnUse");
gradient.setAttribute("x1", "0");
gradient.setAttribute("x2", "200");
gradient.setAttribute("gradientTransform", "translate(0)");

var stop1 = createSVGElement("stop");
stop1.setAttribute("offset", "0");
stop1.setAttribute("stop-color", "green");

var stop2 = createSVGElement("stop");
stop2.setAttribute("offset", "1");
stop2.setAttribute("stop-color", "red");

var animate = createSVGElement("animateTransform");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "gradientTransform");
animate.setAttribute("type", "translate");
animate.setAttribute("from", "0");
animate.setAttribute("to", "200");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("fill", "freeze");

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("fill", "url(#gradient)");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("onclick", "executeTest()");

gradient.appendChild(stop1);
gradient.appendChild(stop2);
gradient.appendChild(animate);

rootSVGElement.appendChild(gradient);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial conditions
    shouldThrow("gradient.gradientTransform.animVal.consolidate()");
    shouldBe("gradient.gradientTransform.animVal.numberOfItems", "1");
    shouldBeCloseEnough("gradient.gradientTransform.animVal.getItem(0).matrix.e", "0");
    shouldBe("gradient.gradientTransform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");

    shouldBe("gradient.gradientTransform.baseVal.numberOfItems", "1");
    shouldBe("gradient.gradientTransform.baseVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBe("gradient.gradientTransform.baseVal.getItem(0).matrix.e", "0");
}

function sample2() {
    // Check half-time conditions
    shouldBe("gradient.gradientTransform.animVal.numberOfItems", "1");
    shouldBe("gradient.gradientTransform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBeCloseEnough("gradient.gradientTransform.animVal.getItem(0).matrix.e", "100");

    shouldBe("gradient.gradientTransform.baseVal.numberOfItems", "1");
    shouldBe("gradient.gradientTransform.baseVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBe("gradient.gradientTransform.baseVal.getItem(0).matrix.e", "0");
}

function sample3() {
    // Check end conditions
    shouldBe("gradient.gradientTransform.animVal.numberOfItems", "1");
    shouldBe("gradient.gradientTransform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBeCloseEnough("gradient.gradientTransform.animVal.getItem(0).matrix.e", "200");

    shouldBe("gradient.gradientTransform.baseVal.numberOfItems", "1");
    shouldBe("gradient.gradientTransform.baseVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_TRANSLATE");
    shouldBe("gradient.gradientTransform.baseVal.getItem(0).matrix.e", "0");
}

function executeTest() {  
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, sample1],
        ["animation", 2.0, sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
