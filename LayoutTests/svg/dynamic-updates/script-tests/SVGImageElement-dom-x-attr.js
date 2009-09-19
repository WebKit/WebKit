// [Name] SVGImageElement-dom-x-attr.js
// [Expected rendering result] image at 0x0 size 200x200 - and a series of PASS messages

description("Tests dynamic updates of the 'x' attribute of the SVGImageElement object")
createSVGTestCase();

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../custom/resources/green-checker.png");
imageElement.setAttribute("preserveAspectRatio", "none");
imageElement.setAttribute("x", "-190");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "200");
rootSVGElement.appendChild(imageElement);

shouldBeEqualToString("imageElement.getAttribute('x')", "-190");

function executeTest() {
    imageElement.setAttribute("x", "0");
    shouldBeEqualToString("imageElement.getAttribute('x')", "0");

    completeTest();
}

startTest(imageElement, 5, 100);

var successfullyParsed = true;
