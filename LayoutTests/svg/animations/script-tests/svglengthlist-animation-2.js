description("Test 'to' animation of SVGLengthList with different LengthTypes.");
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
animate.setAttribute("from", "50 70px 5cm 8pt");
animate.setAttribute("to", "3cm 80 100px 4in");
text.appendChild(animate);

// Setup animation test
function sample1() {
	shouldBe("text.x.animVal.getItem(0).value", "50");
	shouldBe("text.x.animVal.getItem(1).value", "70");
	shouldBe("text.x.animVal.getItem(2).value", "90");
	shouldBe("text.x.animVal.getItem(3).value", "110");
}

function sample2() {
	shouldBeCloseEnough("text.x.animVal.getItem(0).value", "81.7", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(1).value", "75", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(2).value", "144.49", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(3).value", "197.33", 0.01);
}

function sample3() {
	shouldBeCloseEnough("text.x.animVal.getItem(0).value", "113.39", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(1).value", "80", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(2).value", "100", 0.01);
	shouldBeCloseEnough("text.x.animVal.getItem(3).value", "383.99", 0.01);
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
