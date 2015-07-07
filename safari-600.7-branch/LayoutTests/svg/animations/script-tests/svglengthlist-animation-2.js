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
    shouldBe("text.x.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.x.animVal.getItem(0).value", "50");
    shouldBeCloseEnough("text.x.animVal.getItem(1).value", "70");
    shouldBeCloseEnough("text.x.animVal.getItem(2).value", "90");
    shouldBeCloseEnough("text.x.animVal.getItem(3).value", "110");

    shouldBe("text.x.baseVal.numberOfItems", "4");
    shouldBe("text.x.baseVal.getItem(0).value", "50");
    shouldBe("text.x.baseVal.getItem(1).value", "70");
    shouldBe("text.x.baseVal.getItem(2).value", "90");
    shouldBe("text.x.baseVal.getItem(3).value", "110");
}

function sample2() {
    shouldBe("text.x.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.x.animVal.getItem(0).value", "81.7");
    shouldBeCloseEnough("text.x.animVal.getItem(1).value", "75");
    shouldBeCloseEnough("text.x.animVal.getItem(2).value", "144.5");
    shouldBeCloseEnough("text.x.animVal.getItem(3).value", "197.3");

    shouldBe("text.x.baseVal.numberOfItems", "4");
    shouldBe("text.x.baseVal.getItem(0).value", "50");
    shouldBe("text.x.baseVal.getItem(1).value", "70");
    shouldBe("text.x.baseVal.getItem(2).value", "90");
    shouldBe("text.x.baseVal.getItem(3).value", "110");
}

function sample3() {
    shouldBe("text.x.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.x.animVal.getItem(0).value", "113.4");
    shouldBeCloseEnough("text.x.animVal.getItem(1).value", "80");
    shouldBeCloseEnough("text.x.animVal.getItem(2).value", "100");
    shouldBeCloseEnough("text.x.animVal.getItem(3).value", "384");

    shouldBe("text.x.baseVal.numberOfItems", "4");
    shouldBe("text.x.baseVal.getItem(0).value", "50");
    shouldBe("text.x.baseVal.getItem(1).value", "70");
    shouldBe("text.x.baseVal.getItem(2).value", "90");
    shouldBe("text.x.baseVal.getItem(3).value", "110");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
