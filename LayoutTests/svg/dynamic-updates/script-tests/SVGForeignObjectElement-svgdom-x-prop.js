// [Name] SVGForeignObjectElement-svgdom-x-prop.js
// [Expected rendering result] unclipped 'Test passed' text - and a series of PASS messages

description("Tests dynamic updates of the 'x' property of the SVGForeignObjectElement object")
createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("x", "-100");
foreignObjectElement.setAttribute("y", "80");
foreignObjectElement.setAttribute("width", "150");
foreignObjectElement.setAttribute("height", "150");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

shouldBe("foreignObjectElement.x.baseVal.value", "-100");

function executeTest() {
    foreignObjectElement.x.baseVal.value = "100";
    shouldBe("foreignObjectElement.x.baseVal.value", "100");

    completeTest();
}

startTest(foreignObjectElement, 10, 100);

var successfullyParsed = true;
