// [Name] SVGClipPathElement-svgdom-clipPathUnits-prop.js
// [Expected rendering result] green circle - and a series of PASS messages

description("Tests hitTesting on clipped Elements. Clipped areas shouldn't throw a hit.")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var clipPathElement = createSVGElement("clipPath");
clipPathElement.setAttribute("id", "clipper");

var clipRectElement = createSVGElement("rect");
clipRectElement.setAttribute("width", "50");
clipRectElement.setAttribute("height", "100");
clipPathElement.appendChild(clipRectElement);

defsElement.appendChild(clipPathElement);

var backgroundRect = createSVGElement("rect");
backgroundRect.setAttribute("width", "100");
backgroundRect.setAttribute("height", "100");
backgroundRect.setAttribute("fill", "green");
rootSVGElement.appendChild(backgroundRect);

var foregroundRect = createSVGElement("rect");
foregroundRect.setAttribute("width", "100");
foregroundRect.setAttribute("height", "100");
foregroundRect.setAttribute("fill", "green");
foregroundRect.setAttribute("clip-path", "url(#clipper)");
foregroundRect.setAttribute("onclick", "executeBackgroundTest()");
rootSVGElement.appendChild(foregroundRect);

// Two rects are drawn. One in the background and one in the foreground. The rect
// in the foreground gets clipped, so on hittesting, the background rect should
// throw a hit.
function executeBackgroundTest() {
    window.setTimeout("triggerUpdate(75,50)", 0);
    testPassed("Clipped area of rect didn't throw a hit.");
    startTest(foregroundRect, 25, 50);
}

function executeTest() {
    testPassed("Hit thrown on not clipped area of rect.");

    completeTest();
}

executeBackgroundTest();

var successfullyParsed = true;
