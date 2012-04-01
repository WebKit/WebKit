description("Test SVGSpreadMethodType enumeration animations");
createSVGTestCase();

// Setup test document
var gradient = createSVGElement("linearGradient");
gradient.setAttribute("id", "gradient");
rootSVGElement.appendChild(gradient);

var stop = createSVGElement("stop");
stop.setAttribute("offset", "1");
stop.setAttribute("stop-color", "green");
gradient.appendChild(stop);

var feBlend = createSVGElement("feBlend");
feBlend.setAttribute("in", "SourceGraphic");
feBlend.setAttribute("in2", "img");
feBlend.setAttribute("mode", "lighten");
gradient.appendChild(feBlend);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("onclick", "executeTest()");
rect.setAttribute("fill", "url(#gradient)");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rootSVGElement.appendChild(rect);

var animate1 = createSVGElement("animate");
animate1.setAttribute("id", "animation");
animate1.setAttribute("attributeName", "spreadMethod");
animate1.setAttribute("begin", "rect.click");
animate1.setAttribute("dur", "3s");
animate1.setAttribute("values", "pad;reflect;repeat");
animate1.setAttribute("fill", "freeze");
gradient.appendChild(animate1);

// Setup animation test
function sample1() {
    shouldBe("gradient.spreadMethod.animVal", "SVGGradientElement.SVG_SPREADMETHOD_PAD");
    shouldBe("gradient.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_PAD");
}

function sample2() {
    shouldBe("gradient.spreadMethod.animVal", "SVGGradientElement.SVG_SPREADMETHOD_REFLECT");
    shouldBe("gradient.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_PAD");
}

function sample3() {
    shouldBe("gradient.spreadMethod.animVal", "SVGGradientElement.SVG_SPREADMETHOD_REPEAT");
    shouldBe("gradient.spreadMethod.baseVal", "SVGGradientElement.SVG_SPREADMETHOD_PAD");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 0.001, sample1],
        ["animation", 0.999, sample1],
        ["animation", 1.001, sample2],
        ["animation", 1.999, sample2],
        ["animation", 2.001, sample3],
        ["animation", 2.999, sample3],
        ["animation", 3.001, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
