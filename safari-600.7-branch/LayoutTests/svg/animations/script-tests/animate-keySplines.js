description("Testing correct parsing of keySplines.");
createSVGTestCase();

// Setup test document
var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("fill", "green");
rect.setAttribute("x", "0");
rect.setAttribute("y", "0");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "height");
animate.setAttribute("calcMode", "splines");
animate.setAttribute("keySplines", "0 ,0  1 , 1  ;   0 0 , 1 ,    1;  .75 , 0 , 0 , .75");
animate.setAttribute("values", "200;167;111;0");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "9s");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBeCloseEnough("rect.height.animVal.value", "167");
    shouldBe("rect.height.baseVal.value", "100");
}

function sample2() {
    // Check half-time conditions
    shouldBeCloseEnough("rect.height.animVal.value", "111");
    shouldBe("rect.height.baseVal.value", "100");
}

function sample3() {
    // Check just before-end conditions
    shouldBeCloseEnough("rect.height.animVal.value", "0");
    shouldBe("rect.height.baseVal.value", "100");
}

function sample4() {
    // Check end conditions
    shouldBeCloseEnough("rect.height.animVal.value", "100");
    shouldBe("rect.height.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 3.0,   sample1],
        ["animation", 6.0,   sample2],
        ["animation", 8.999, sample3],
        ["animation", 9.001, sample4]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
