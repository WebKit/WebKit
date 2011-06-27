description("Test 'to' animation of SVGLengthList with LengthType number.");
createSVGTestCase();

// Setup test document
var text = createSVGElement("text");
text.setAttribute("id", "text");
text.textContent = "ABCD";
text.setAttribute("x", "50 70 90 110");
text.setAttribute("y", "50");
text.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(text);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "x");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "50 70 90 110");
animate.setAttribute("to", "60 90 120 150");
text.appendChild(animate);

// Setup animation test
function sample1() {
	shouldBe("text.x.animVal.getItem(0).value", "50");
	shouldBe("text.x.animVal.getItem(1).value", "70");
	shouldBe("text.x.animVal.getItem(2).value", "90");
	shouldBe("text.x.animVal.getItem(3).value", "110");
}

function sample2() {
	shouldBe("text.x.animVal.getItem(0).value", "55");
	shouldBe("text.x.animVal.getItem(1).value", "80");
	shouldBe("text.x.animVal.getItem(2).value", "105");
	shouldBe("text.x.animVal.getItem(3).value", "130");
}

function sample3() {
	shouldBeCloseEnough("text.x.animVal.getItem(0).value", "60", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(1).value", "90", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(2).value", "120", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(3).value", "150", 0.01);
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
