description("Test for SVGNumber animation on SVG DOM properties.");
createSVGTestCase();

// Setup test document
var gradient = createSVGElement("linearGradient");
gradient.setAttribute("id", "gradient");

var stop1 = createSVGElement("stop");
stop1.setAttribute("offset", "0");
stop1.setAttribute("stop-color", "green");
gradient.appendChild(stop1);

var stop2 = createSVGElement("stop");
stop2.setAttribute("offset", "1");
stop2.setAttribute("stop-color", "red");
gradient.appendChild(stop2);

var defsElement = createSVGElement("defs");
defsElement.appendChild(gradient);
rootSVGElement.appendChild(defsElement);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("x", "0");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "url(#gradient)");
rect.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeType", "XML");
animate.setAttribute("attributeName", "offset");
animate.setAttribute("begin", "rect.click");
animate.setAttribute("dur", "4s");
animate.setAttribute("from", "0");
animate.setAttribute("to", "1");
animate.setAttribute("fill", "freeze");
stop1.appendChild(animate);

rootSVGElement.appendChild(rect);

// Setup animation test
function sample1() {
    shouldBeCloseEnough("stop1.offset.animVal", "0");
    shouldBe("stop1.offset.baseVal", "0");
}

function sample2() {
    shouldBeCloseEnough("stop1.offset.animVal", "0.5");
    shouldBe("stop1.offset.baseVal", "0");
}

function sample3() {
    shouldBeCloseEnough("stop1.offset.animVal", "1");
    shouldBe("stop1.offset.baseVal", "0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0, sample1],
        ["animation", 2.0, sample2],
        ["animation", 4.0, sample3]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
