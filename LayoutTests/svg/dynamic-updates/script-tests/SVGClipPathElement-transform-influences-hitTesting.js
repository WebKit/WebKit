// [Name] SVGClipPathElement-transform-influences-hitTesting.js
// [Expected rendering result] green circle - and a series of PASS messages

description("Tests hitTesting on clipped Elements. Clip-path gets transformed.")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var clipPathElement = createSVGElement("clipPath");
clipPathElement.setAttribute("id", "clipper");

var clipRectElement = createSVGElement("rect");
clipRectElement.setAttribute("width", "5");
clipRectElement.setAttribute("height", "5");
clipRectElement.setAttribute("transform", "scale(20)");
clipPathElement.appendChild(clipRectElement);

defsElement.appendChild(clipPathElement);

var foregroundRect = createSVGElement("rect");
foregroundRect.setAttribute("width", "100");
foregroundRect.setAttribute("height", "100");
foregroundRect.setAttribute("fill", "green");
foregroundRect.setAttribute("clip-path", "url(#clipper)");
foregroundRect.setAttribute("onclick", "testPassed('Hit thrown on not clipped area of rect.'); completeTest();");
rootSVGElement.appendChild(foregroundRect);

// The clipPath gets scaled by 20. This should influence the hit testing,
// since the area of the clipped content is affected as well. 
function repaintTest() {
    clickAt(75, 50);
}

var successfullyParsed = true;
