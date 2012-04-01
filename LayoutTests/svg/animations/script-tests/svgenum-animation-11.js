description("Test BlendModeType enumeration animations");
createSVGTestCase();

// Setup test document
var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
rootSVGElement.appendChild(filter);

var feFlood = createSVGElement("feFlood");
feFlood.setAttribute("in", "SourceGraphic");
feFlood.setAttribute("flood-color", "green");
feFlood.setAttribute("flood-opacity", "0.5");
feFlood.setAttribute("result", "img");
filter.appendChild(feFlood);

var feBlend = createSVGElement("feBlend");
feBlend.setAttribute("in", "SourceGraphic");
feBlend.setAttribute("in2", "img");
feBlend.setAttribute("mode", "lighten");
filter.appendChild(feBlend);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("onclick", "executeTest()");
rect.setAttribute("filter", "url(#filter)");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rootSVGElement.appendChild(rect);

var animate1 = createSVGElement("animate");
animate1.setAttribute("id", "animation");
animate1.setAttribute("attributeName", "mode");
animate1.setAttribute("begin", "rect.click");
animate1.setAttribute("dur", "5s");
animate1.setAttribute("values", "normal;multiply;screen;darken;lighten");
animate1.setAttribute("fill", "freeze");
feBlend.appendChild(animate1);

// Setup animation test
function sample1() {
    shouldBe("feBlend.mode.animVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
    shouldBe("feBlend.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
}

function sample2() {
    shouldBe("feBlend.mode.animVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_NORMAL");
    shouldBe("feBlend.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
}

function sample3() {
    shouldBe("feBlend.mode.animVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_MULTIPLY");
    shouldBe("feBlend.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
}

function sample4() {
    shouldBe("feBlend.mode.animVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_SCREEN");
    shouldBe("feBlend.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
}

function sample5() {
    shouldBe("feBlend.mode.animVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_DARKEN");
    shouldBe("feBlend.mode.baseVal", "SVGFEBlendElement.SVG_FEBLEND_MODE_LIGHTEN");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.001, sample2],
        ["animation", 0.999, sample2],
        ["animation", 1.001, sample3],
        ["animation", 1.999, sample3],
        ["animation", 2.001, sample4],
        ["animation", 2.999, sample4],
        ["animation", 3.001, sample5],
        ["animation", 3.999, sample5],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
