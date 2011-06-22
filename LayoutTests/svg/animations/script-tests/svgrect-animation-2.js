description("Tests from-by SVGRect animation.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "green");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

rootSVGElement.setAttribute("id", "svg");
rootSVGElement.setAttribute("viewBox", "0 0 100 100");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "viewBox");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "0 0 100 100");
animate.setAttribute("by", "50 50 50 50");
rootSVGElement.appendChild(animate);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rootSVGElement.viewBox.animVal.x", "0");
    shouldBe("rootSVGElement.viewBox.animVal.y", "0");
    shouldBe("rootSVGElement.viewBox.animVal.width", "100");
    shouldBe("rootSVGElement.viewBox.animVal.height", "100");

    shouldBe("rootSVGElement.viewBox.baseVal.x", "0");
    shouldBe("rootSVGElement.viewBox.baseVal.y", "0");
    shouldBe("rootSVGElement.viewBox.baseVal.width", "100");
    shouldBe("rootSVGElement.viewBox.baseVal.height", "100");
}

function sample2() {
    shouldBe("rootSVGElement.viewBox.animVal.x", "25");
    shouldBe("rootSVGElement.viewBox.animVal.y", "25");
    shouldBe("rootSVGElement.viewBox.animVal.width", "125");
    shouldBe("rootSVGElement.viewBox.animVal.height", "125");

    shouldBe("rootSVGElement.viewBox.baseVal.x", "25");
    shouldBe("rootSVGElement.viewBox.baseVal.y", "25");
    shouldBe("rootSVGElement.viewBox.baseVal.width", "125");
    shouldBe("rootSVGElement.viewBox.baseVal.height", "125");
}

function sample3() {
    shouldBeCloseEnough("rootSVGElement.viewBox.animVal.x", "50", 0.01);
    shouldBeCloseEnough("rootSVGElement.viewBox.animVal.y", "50", 0.01);
    shouldBeCloseEnough("rootSVGElement.viewBox.animVal.width", "150", 0.01);
    shouldBeCloseEnough("rootSVGElement.viewBox.animVal.height", "150", 0.01);

    shouldBeCloseEnough("rootSVGElement.viewBox.baseVal.x", "50", 0.01);
    shouldBeCloseEnough("rootSVGElement.viewBox.baseVal.y", "50", 0.01);
    shouldBeCloseEnough("rootSVGElement.viewBox.baseVal.width", "150", 0.01);
    shouldBeCloseEnough("rootSVGElement.viewBox.baseVal.height", "150", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "svg", sample1],
        ["animation", 2.0,    "svg", sample2],
        ["animation", 3.9999, "svg", sample3],
        ["animation", 4.0 ,   "svg", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 30)", 0);
var successfullyParsed = true;
