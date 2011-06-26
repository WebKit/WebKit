description("Test 'by' animation of SVGNumberOptionalNumber.");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
filter.setAttribute("x", "-30%");
filter.setAttribute("y", "-30%");
filter.setAttribute("width", "160%");
filter.setAttribute("height", "160%");
defs.appendChild(filter);

var feGaussianBlur = createSVGElement("feGaussianBlur");
feGaussianBlur.setAttribute("id", "blur");
feGaussianBlur.setAttribute("stdDeviation", "5");
filter.appendChild(feGaussianBlur);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "50");
rect.setAttribute("y", "50");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("fill", "green");
rect.setAttribute("filter", "url(#filter)");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "stdDeviation");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "5");
animate.setAttribute("by", "10");
feGaussianBlur.appendChild(animate);

// Setup animation test
function sample1() {
	shouldBe("feGaussianBlur.stdDeviationX.animVal", "5");
	shouldBe("feGaussianBlur.stdDeviationY.animVal", "5");
}

function sample2() {
	shouldBeCloseEnough("feGaussianBlur.stdDeviationX.animVal", "10", 0.01);
	shouldBeCloseEnough("feGaussianBlur.stdDeviationY.animVal", "10", 0.01);
}

function sample3() {
	shouldBeCloseEnough("feGaussianBlur.stdDeviationX.animVal", "15", 0.01);
	shouldBeCloseEnough("feGaussianBlur.stdDeviationY.animVal", "15", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "blur", sample1],
        ["animation", 2.0,    "blur", sample2],
        ["animation", 3.9999, "blur", sample3],
        ["animation", 4.0 ,   "blur", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(60, 60)", 0);
var successfullyParsed = true;
