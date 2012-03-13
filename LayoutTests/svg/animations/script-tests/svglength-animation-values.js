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
    shouldBeCloseEnough("rect.width.animVal.value", "100");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample2() {
    shouldBeCloseEnough("rect.width.animVal.value", "130");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample3() {
    shouldBeCloseEnough("rect.width.animVal.value", "151.2");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample4() {
    shouldBeCloseEnough("rect.width.animVal.value", "576");
    shouldBe("rect.width.baseVal.value", "100");
}

function sample5() {
    shouldBeCloseEnough("rect.width.animVal.value", "267");
    shouldBe("rect.width.baseVal.value", "100");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.0,   sample2],
        ["animation", 2.0,   sample3],
        ["animation", 3.0,   sample4],
        ["animation", 3.999, sample5],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
