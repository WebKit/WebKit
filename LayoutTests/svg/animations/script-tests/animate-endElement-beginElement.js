description("Tests animation beginElement command's restarting capability after endElement.");
createSVGTestCase();

// Setup test document

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "50px");
rect.setAttribute("height", "50px");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");

var animateX = createSVGElement("animate");
animateX.setAttribute("id", "animateX");
animateX.setAttribute("attributeName", "x");
animateX.setAttribute("from", "0");
animateX.setAttribute("to", "100");
animateX.setAttribute("dur", "2s");
animateX.setAttribute("begin", "indefinite");
animateX.setAttribute("fill", "freeze");
rect.appendChild(animateX);
rootSVGElement.appendChild(rect);

// Setup animation test

function sample1() {
    // Check half-time conditions
    shouldBeCloseEnough("rect.x.animVal.value", "50");
    shouldBe("rect.x.baseVal.value", "0");
}

function executeTest() {
    // Start animating, and stop it again after 100ms.
    animateX.beginElement();
    setTimeout(end, 100);
}

function end() {
    // Stop animating, and restart it in 100ms.
    animateX.endElement();
    setTimeout(begin, 100);
}

function begin() {
    // Once the animation is running again, sample it.
    animateX.beginElement();

    setTimeout(function() {
        const expectedValues = [
            // [animationId, time, sampleCallback]
            ["animateX", 1.0, sample1]
        ];
        runAnimationTest(expectedValues);
    }, 100);
}

window.clickX = 40;
window.clickY = 40;
var successfullyParsed = true;
