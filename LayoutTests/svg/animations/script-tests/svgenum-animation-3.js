description("Test SVGLengthAdjustType enumeration animations");
createSVGTestCase();

// Setup test document
var text = createSVGElement("text");
text.setAttribute("id", "text");
text.setAttribute("y", "50");
text.setAttribute("textLength", "200");
text.textContent = "Stretched text";
text.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(text);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "lengthAdjust");
animate.setAttribute("begin", "text.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "spacing");
animate.setAttribute("to", "spacingAndGlyphs");
animate.setAttribute("fill", "freeze");
text.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBe("text.lengthAdjust.animVal", "SVGTextContentElement.LENGTHADJUST_SPACING");
    shouldBe("text.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACING");
}

function sample2() {
    shouldBe("text.lengthAdjust.animVal", "SVGTextContentElement.LENGTHADJUST_SPACINGANDGLYPHS");
    shouldBe("text.lengthAdjust.baseVal", "SVGTextContentElement.LENGTHADJUST_SPACING");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample1],
        ["animation", 2.001, sample2],
        ["animation", 3.999, sample2],
        ["animation", 4.001, sample2]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
