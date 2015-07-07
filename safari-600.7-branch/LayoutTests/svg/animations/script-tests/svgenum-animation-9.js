description("Test SVGTextPathSpacingType/SVGTextPathMethodType enumeration animations");
createSVGTestCase();

// Setup test document
var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("d", "M 50 50 L 200 50");
rootSVGElement.appendChild(path);

var text = createSVGElement("text");
text.setAttribute("id", "text");
text.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(text);

var textPath = createSVGElement("textPath");
textPath.setAttributeNS(xlinkNS, "xlink:href", "#path");
textPath.setAttribute("spacing", "auto");
textPath.setAttribute("method", "align");
textPath.textContent = "test";
text.appendChild(textPath);

var animate1 = createSVGElement("animate");
animate1.setAttribute("id", "animation");
animate1.setAttribute("attributeName", "spacing");
animate1.setAttribute("begin", "text.click");
animate1.setAttribute("dur", "4s");
animate1.setAttribute("from", "auto");
animate1.setAttribute("to", "exact");
animate1.setAttribute("fill", "freeze");
textPath.appendChild(animate1);

var animate2 = createSVGElement("animate");
animate2.setAttribute("attributeName", "method");
animate2.setAttribute("begin", "text.click");
animate2.setAttribute("dur", "4s");
animate2.setAttribute("from", "align");
animate2.setAttribute("to", "stretch");
animate2.setAttribute("fill", "freeze");
textPath.appendChild(animate2);

// Setup animation test
function sample1() {
    shouldBe("textPath.method.animVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN");
    shouldBe("textPath.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN");

    shouldBe("textPath.spacing.animVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO");
    shouldBe("textPath.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO");
}

function sample2() {
    shouldBe("textPath.method.animVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_STRETCH");
    shouldBe("textPath.method.baseVal", "SVGTextPathElement.TEXTPATH_METHODTYPE_ALIGN");

    shouldBe("textPath.spacing.animVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_EXACT");
    shouldBe("textPath.spacing.baseVal", "SVGTextPathElement.TEXTPATH_SPACINGTYPE_AUTO");
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
