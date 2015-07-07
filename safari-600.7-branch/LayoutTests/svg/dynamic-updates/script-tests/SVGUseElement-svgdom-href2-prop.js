// [Name] SVGUseElement-svgdom-href2-prop.js
// [Expected rendering result] A use element first with an external then with an internal referenced document - and a series of PASS messages

description("Tests dynamic updates of the 'href' property of the SVGUseElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var useElement = createSVGElement("use");
useElement.setAttribute("x", "10");
useElement.setAttribute("y", "10");
useElement.setAttribute("externalResourcesRequired", "true");
useElement.setAttribute("onload", "externalLoadDone()");

var rectElement = createSVGElement("rect");
rectElement.setAttribute("id", "MyRect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "0");
rectElement.setAttribute("width", "64");
rectElement.setAttribute("height", "64");
rectElement.setAttribute("fill", "green");

defsElement.appendChild(rectElement);

rootSVGElement.setAttribute("height", "200");
rootSVGElement.appendChild(useElement);

function repaintTest() {
    // Start loading external resource, wait for it, then switch back to internal.
    useElement.setAttributeNS(xlinkNS, "xlink:href", "../custom/resources/rgb.svg#R");
}

function externalLoadDone() {
    useElement.href.baseVal = "#MyRect";
    shouldBeEqualToString("useElement.href.baseVal", "#MyRect");

    completeTest();
}

var successfullyParsed = true;
