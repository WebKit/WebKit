// [Name] SVGLineElement-svgdom-x2-prop.js
// [Expected rendering result] green line from 10x10 to 200x200 - and a series of PASS messages

description("Tests dynamic updates of the 'x2' property of the SVGLineElement object")
createSVGTestCase();

var lineElement = createSVGElement("line");
lineElement.setAttribute("x1", "10");
lineElement.setAttribute("y1", "10");
lineElement.setAttribute("x2", "100");
lineElement.setAttribute("y2", "200");
lineElement.setAttribute("stroke", "green");
lineElement.setAttribute("stroke-width", "10");
rootSVGElement.appendChild(lineElement);

shouldBe("lineElement.x2.baseVal.value", "100");

function executeTest() {
    lineElement.x2.baseVal.value = 200;
    shouldBe("lineElement.x2.baseVal.value", "200");

    completeTest();
}

startTest(lineElement, 11, 11);

var successfullyParsed = true;
