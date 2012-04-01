description("Test SVGStitchOptions/TurbulenceType enumeration animations");
createSVGTestCase();

// Setup test document
var defs = createSVGElement("defs");
rootSVGElement.appendChild(defs);

var filter = createSVGElement("filter");
filter.setAttribute("id", "filter");
filter.setAttribute("filterUnits", "userSpaceOnUse");
filter.setAttribute("x", "0");
filter.setAttribute("y", "0");
filter.setAttribute("width", "700");
filter.setAttribute("height", "200");
defs.appendChild(filter);

var turbulence = createSVGElement("feTurbulence");
turbulence.setAttribute("in", "foo");
turbulence.setAttribute("baseFrequency", "0.05");
turbulence.setAttribute("numOctaves", "3");
turbulence.setAttribute("seed", "5");
turbulence.setAttribute("stitchTiles", "stitch");
turbulence.setAttribute("type", "fractalNoise");
filter.appendChild(turbulence);

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "100");
rect.setAttribute("height", "100");
rect.setAttribute("fill", "#408067");
rect.setAttribute("filter", "url(#filter)");
rect.setAttribute("onclick", "executeTest()");
rootSVGElement.appendChild(rect);

var animate1 = createSVGElement("animate");
animate1.setAttribute("id", "animation");
animate1.setAttribute("attributeName", "type");
animate1.setAttribute("begin", "rect.click");
animate1.setAttribute("dur", "4s");
animate1.setAttribute("from", "fractalNoise");
animate1.setAttribute("to", "turbulence");
turbulence.appendChild(animate1);

var animate2 = createSVGElement("animate");
animate2.setAttribute("attributeName", "stitchTiles");
animate2.setAttribute("begin", "rect.click");
animate2.setAttribute("dur", "4s");
animate2.setAttribute("from", "stitch");
animate2.setAttribute("to", "noStitch");
turbulence.appendChild(animate2);

// Setup animation test
function sample1() {
    shouldBe("turbulence.type.animVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");
    shouldBe("turbulence.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");

    shouldBe("turbulence.stitchTiles.animVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH");
    shouldBe("turbulence.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH");
}

function sample2() {
    shouldBe("turbulence.type.animVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_TURBULENCE");
    shouldBe("turbulence.type.baseVal", "SVGFETurbulenceElement.SVG_TURBULENCE_TYPE_FRACTALNOISE");

    shouldBe("turbulence.stitchTiles.animVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_NOSTITCH");
    shouldBe("turbulence.stitchTiles.baseVal", "SVGFETurbulenceElement.SVG_STITCHTYPE_STITCH");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample1],
        ["animation", 2.001, sample2],
        ["animation", 3.999, sample2],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
