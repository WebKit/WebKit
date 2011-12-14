// [Name] SVGLinearGradientElement-svgdom-x1-prop.js
// [Expected rendering result] green ellipse, no red visible - and a series of PASS messages

description("Tests dynamic updates of the 'x1' property of the SVGLinearGradientElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "url(#gradient)");

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var linearGradientElement = createSVGElement("linearGradient");
linearGradientElement.setAttribute("id", "gradient");
linearGradientElement.setAttribute("x1", "100%");

var firstStopElement = createSVGElement("stop");
firstStopElement.setAttribute("offset", "0");
firstStopElement.setAttribute("stop-color", "red");
linearGradientElement.appendChild(firstStopElement);

var lastStopElement = createSVGElement("stop");
lastStopElement.setAttribute("offset", "1");
lastStopElement.setAttribute("stop-color", "green");
linearGradientElement.appendChild(lastStopElement);

defsElement.appendChild(linearGradientElement);
rootSVGElement.appendChild(ellipseElement);

shouldBeEqualToString("linearGradientElement.x1.baseVal.valueAsString", "100%");

function executeTest() {
    linearGradientElement.x1.baseVal.valueAsString = "200%";
    shouldBeEqualToString("linearGradientElement.x1.baseVal.valueAsString", "200%");

    completeTest();
}

startTest(ellipseElement, 150, 150);

var successfullyParsed = true;
