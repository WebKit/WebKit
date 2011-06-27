description("Test 'to' animation of SVGLengthList with different count of items.");
createSVGTestCase();

// Setup test document
var text = createSVGElement("text");
text.setAttribute("id", "text");
text.textContent = "ABCD";
text.setAttribute("x", "50");
text.setAttribute("y", "50");
text.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(text);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "x");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "50");
animate.setAttribute("to", "70 80 90 110");
text.appendChild(animate);

// Setup animation test
function sample1() {
	shouldBe("text.x.animVal.numberOfItems", "1");
	shouldBe("text.x.animVal.getItem(0).value", "50");
}

function sample2() {
	shouldBe("text.x.animVal.numberOfItems", "4");
	shouldBeCloseEnough("text.x.animVal.getItem(0).value", "70", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(1).value", "80", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(2).value", "90", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(3).value", "110", 0.01);
}

function sample3() {
	shouldBe("text.x.animVal.numberOfItems", "4");
	shouldBeCloseEnough("text.x.animVal.getItem(0).value", "70", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(1).value", "80", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(2).value", "90", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(3).value", "110", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "text", sample1],
        ["animation", 2.0,    "text", sample2],
        ["animation", 3.9999, "text", sample3],
        ["animation", 4.0 ,   "text", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(51, 49)", 0);
var successfullyParsed = true;
