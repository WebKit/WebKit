description("Test SVGLength animation set with 'values', different units and different count of spaces.");
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
animate.setAttribute("values", "100px;  130  ;4cm  ;6in; 200pt");
rect.appendChild(animate);
rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "130", 0.01);
    shouldBeCloseEnough("rect.width.baseVal.value", "130", 0.01);
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "151.18", 0.01);
    shouldBeCloseEnough("rect.width.baseVal.value", "151.18", 0.01);
}

function sample4() {
    shouldBeCloseEnough("rect.width.animVal.value", "576", 0.01);
    shouldBeCloseEnough("rect.width.baseVal.value", "576", 0.01);
}

function sample5() {
    shouldBeCloseEnough("rect.width.animVal.value", "266.7", 0.01);
    shouldBeCloseEnough("rect.width.baseVal.value", "266.7", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "rect", sample1],
        ["animation", 1.0,    "rect", sample2],
        ["animation", 2.0,    "rect", sample3],
        ["animation", 3.0,    "rect", sample4],
        ["animation", 3.9999, "rect", sample5],
        ["animation", 4.0 ,   "rect", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 30)", 0);
var successfullyParsed = true;
