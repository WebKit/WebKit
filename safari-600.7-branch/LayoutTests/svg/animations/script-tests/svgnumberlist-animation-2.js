description("Test 'by' animation of SVGNumberList on 'rotate' attribute of text.");
createSVGTestCase();

// Setup test document
var text = createSVGElement("text");
text.textContent = "ABCD";
text.setAttribute("x", "40 60 80 100");
text.setAttribute("y", "60");
text.setAttribute("rotate", "15");
text.setAttribute("fill", "green");
text.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "rotate");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "0 45 90 135");
animate.setAttribute("by", "45 45 45 45");
text.appendChild(animate);
rootSVGElement.appendChild(text);

// Setup animation test
function sample1() {
    shouldBe("text.rotate.animVal.numberOfItems", "1");
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "15");

    shouldBe("text.rotate.baseVal.numberOfItems", "1");
    shouldBe("text.rotate.baseVal.getItem(0).value", "15");
}

function sample2() {
    shouldBe("text.rotate.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "0");
    shouldBeCloseEnough("text.rotate.animVal.getItem(1).value", "45");
    shouldBeCloseEnough("text.rotate.animVal.getItem(2).value", "90");
    shouldBeCloseEnough("text.rotate.animVal.getItem(3).value", "135");

    shouldBe("text.rotate.baseVal.numberOfItems", "1");
    shouldBe("text.rotate.baseVal.getItem(0).value", "15");
}

function sample3() {
    shouldBe("text.rotate.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "22.5");
    shouldBeCloseEnough("text.rotate.animVal.getItem(1).value", "67.5");
    shouldBeCloseEnough("text.rotate.animVal.getItem(2).value", "112.5");
    shouldBeCloseEnough("text.rotate.animVal.getItem(3).value", "157.5");

    shouldBe("text.rotate.baseVal.numberOfItems", "1");
    shouldBe("text.rotate.baseVal.getItem(0).value", "15");
}

function sample4() {
    shouldBe("text.rotate.animVal.numberOfItems", "4");
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "45");
    shouldBeCloseEnough("text.rotate.animVal.getItem(1).value", "90");
    shouldBeCloseEnough("text.rotate.animVal.getItem(2).value", "135");
    shouldBeCloseEnough("text.rotate.animVal.getItem(3).value", "180");

    shouldBe("text.rotate.baseVal.numberOfItems", "1");
    shouldBe("text.rotate.baseVal.getItem(0).value", "15");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.001, sample2],
        ["animation", 2.0,   sample3],
        ["animation", 3.999, sample4],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
