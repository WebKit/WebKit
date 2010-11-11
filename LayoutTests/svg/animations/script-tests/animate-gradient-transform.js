// FIXME: This test will become useful once we have basic animVal support. For now it's just testing the SVG animation test infrastructure
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
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("fill", "freeze");

var rect = createSVGElement("rect");
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
    // FIXME: Add animVal support. Animates baseVal at the moment.
    // Check initial conditions
    shouldBe("gradient.gradientTransform.baseVal.consolidate().matrix.e", "0");
    shouldThrow("gradient.gradientTransform.animVal.consolidate().matrix.e");
}

function sample2() {
    // FIXME: Add animVal support. Animates baseVal at the moment.
    // Check half-time conditions
    shouldBe("gradient.gradientTransform.baseVal.consolidate().matrix.e", "100");
    shouldThrow("gradient.gradientTransform.animVal.consolidate().matrix.e");
}

function sample3() {
    // FIXME: Add animVal support. Animates baseVal at the moment.
    // Check end conditions
    shouldBe("gradient.gradientTransform.baseVal.consolidate().matrix.e", "200");
    shouldThrow("gradient.gradientTransform.animVal.consolidate().matrix.e");
}

function executeTest() {  
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "gradient", sample1],
        ["animation", 2.0,    "gradient", sample2],
        ["animation", 4.0,    "gradient", sample3],
    ];

    animate.beginElement();
    runAnimationTest(expectedValues);
  
}

// Begin test async
window.setTimeout("triggerUpdate(15, 30)", 0);
var successfullyParsed = true;
