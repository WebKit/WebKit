description("Test SVGUnitTypes enumeration animations");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var pattern = createSVGElement("pattern");
pattern.setAttribute("id", "pattern");
pattern.setAttribute("patternUnits", "userSpaceOnUse");
pattern.setAttribute("patternContentUnits", "userSpaceOnUse");
pattern.setAttribute("width", "50");
pattern.setAttribute("height", "50");
defs.appendChild(pattern);

var patternChild = createSVGElement("rect");
patternChild.setAttribute("width", "1");
patternChild.setAttribute("height", "1");
patternChild.setAttribute("fill", "green");
pattern.appendChild(patternChild);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "url(#pattern)");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "patternContentUnits");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "userSpaceOnUse");
animate.setAttribute("to", "objectBoundingBox");
animate.setAttribute("fill", "freeze");
pattern.appendChild(animate);

// Setup animation test
function sample1() {
    shouldBe("pattern.patternContentUnits.animVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
    shouldBe("pattern.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
}

function sample2() {
    shouldBe("pattern.patternContentUnits.animVal", "SVGUnitTypes.SVG_UNIT_TYPE_OBJECTBOUNDINGBOX");
    shouldBe("pattern.patternContentUnits.baseVal", "SVGUnitTypes.SVG_UNIT_TYPE_USERSPACEONUSE");
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
