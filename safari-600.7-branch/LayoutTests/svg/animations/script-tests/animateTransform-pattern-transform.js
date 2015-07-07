description("Tests animation of 'patternTransform'. Should result in a 100x100 rect and only PASS messages.");
createSVGTestCase();

// Setup test document

var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var pattern = createSVGElement("pattern");
pattern.setAttribute("id", "pattern");
pattern.setAttribute("width", "200");
pattern.setAttribute("height", "200");

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "200");
rect.setAttribute("height", "200");
rect.setAttribute("fill", "url(#pattern)");
rect.setAttribute("onclick", "executeTest()");

var patternRect = createSVGElement("rect");
patternRect.setAttribute("id", "patternRect");
patternRect.setAttribute("width", "100");
patternRect.setAttribute("height", "100");
patternRect.setAttribute("fill", "green");
pattern.appendChild(patternRect);

var animate = createSVGElement("animateTransform");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "patternTransform");
animate.setAttribute("type", "scale");
animate.setAttribute("from", "1");
animate.setAttribute("to", "2");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
pattern.appendChild(animate);
defs.appendChild(pattern);

rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("pattern.patternTransform.animVal.numberOfItems", "0");
    shouldBe("pattern.patternTransform.baseVal.numberOfItems", "0");
}

function sample2() {
    shouldBe("pattern.patternTransform.animVal.numberOfItems", "1");
    shouldBe("pattern.patternTransform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("pattern.patternTransform.animVal.getItem(0).matrix.a", "1");

    shouldBe("pattern.patternTransform.baseVal.numberOfItems", "0");
}

function sample3() {
    // Check half-time conditions
    shouldBe("pattern.patternTransform.animVal.numberOfItems", "1");
    shouldBe("pattern.patternTransform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("pattern.patternTransform.animVal.getItem(0).matrix.a", "1.5");

    shouldBe("pattern.patternTransform.baseVal.numberOfItems", "0");
}

function sample4() {
    // Check half-time conditions
    shouldBe("pattern.patternTransform.animVal.numberOfItems", "1");
    shouldBe("pattern.patternTransform.animVal.getItem(0).type", "SVGTransform.SVG_TRANSFORM_SCALE");
    shouldBeCloseEnough("pattern.patternTransform.animVal.getItem(0).matrix.a", "2");

    shouldBe("pattern.patternTransform.baseVal.numberOfItems", "0");
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
