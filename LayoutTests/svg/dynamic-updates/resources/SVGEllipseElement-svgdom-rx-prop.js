// [Name] SVGEllipseElement-svgdom-rx-prop.js
// [Expected rendering result] green ellipse - and a series of PASS mesages

description("Tests dynamic updates of the 'rx' property of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "0");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBe("ellipseElement.rx.baseVal.value", "0");

function executeTest() {
    ellipseElement.rx.baseVal.value = "100";
    shouldBe("ellipseElement.rx.baseVal.value", "100");

    waitForClickEvent(ellipseElement); 
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
