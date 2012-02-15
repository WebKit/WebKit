// [Name] SVGAElement-svgdom-href-prop.js
// [Expected rendering result] green 300x300 rectangle - created by resources/linkTarget.svg
// [Test purpose] If SVGAElement does not react properly on 'xlink:href' SVG DOM attribute changes
//                linkTarget.svg won't be opened, but a nonexistant resource -> notifyDone()
//                is never called -> DRT will hang.

description("Tests dynamic updates of the 'xlink:href' property of the SVGAElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "300");
rectElement.setAttribute("height", "300");
rectElement.setAttribute("fill", "red");

var anchorElement = createSVGElement("a");
anchorElement.setAttributeNS(xlinkNS, "xlink:href", "notexistant");
anchorElement.appendChild(rectElement);

rootSVGElement.appendChild(anchorElement);

function repaintTest() {
    anchorElement.href.baseVal = "resources/linkTarget.svg";
    clickAt(150, 200);
}

var successfullyParsed = true;
