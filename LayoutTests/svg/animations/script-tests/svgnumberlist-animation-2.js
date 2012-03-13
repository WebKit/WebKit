description("Test 'by' animation of SVGNumberList on 'rotate' attribute of text.");
createSVGTestCase();

// Setup test document
var text = createSVGElement("text");
text.textContent = "ABCD";
text.setAttribute("id", "text");
text.setAttribute("x", "40 60 80 100");
text.setAttribute("y", "60");
text.setAttribute("rotate", "0 45 90 153");
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
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "0");
    shouldBeCloseEnough("text.rotate.animVal.getItem(1).value", "45");
    shouldBeCloseEnough("text.rotate.animVal.getItem(2).value", "90");
    shouldBeCloseEnough("text.rotate.animVal.getItem(3).value", "153");
}

function sample2() {
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "22.5");
    shouldBeCloseEnough("text.rotate.animVal.getItem(1).value", "67.5");
    shouldBeCloseEnough("text.rotate.animVal.getItem(2).value", "112.5");
    shouldBeCloseEnough("text.rotate.animVal.getItem(3).value", "157.5");
}

function sample3() {
    shouldBeCloseEnough("text.rotate.animVal.getItem(0).value", "45");
    shouldBeCloseEnough("text.rotate.animVal.getItem(1).value", "90");
    shouldBeCloseEnough("text.rotate.animVal.getItem(2).value", "135");
    shouldBeCloseEnough("text.rotate.animVal.getItem(3).value", "180");
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
