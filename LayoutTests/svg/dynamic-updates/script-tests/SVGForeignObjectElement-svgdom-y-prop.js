// [Name] SVGForeignObjectElement-svgdom-y-prop.js
// [Expected rendering result] unclipped 'Test passed' text - and a series of PASS messages

description("Tests dynamic updates of the 'y' property of the SVGForeignObjectElement object")
createSVGTestCase();

var foreignObjectElement = createSVGElement("foreignObject");
foreignObjectElement.setAttribute("x", "100");
foreignObjectElement.setAttribute("y", "-80");
foreignObjectElement.setAttribute("width", "150");
foreignObjectElement.setAttribute("height", "150");

var htmlDivElement = document.createElementNS(xhtmlNS, "xhtml:div");
htmlDivElement.setAttribute("style", "background-color: green; color: white; text-align: center");
htmlDivElement.textContent = "Test passed";

foreignObjectElement.appendChild(htmlDivElement);
rootSVGElement.appendChild(foreignObjectElement);

shouldBe("foreignObjectElement.y.baseVal.value", "-80");

function executeTest() {
    foreignObjectElement.y.baseVal.value = "80";
    shouldBe("foreignObjectElement.y.baseVal.value", "80");

    completeTest();
}

startTest(foreignObjectElement, 150, 50);

var successfullyParsed = true;
