// [Name] SVGTRefElement-dom-href-attr.js
// [Expected rendering result] 'Test passed' message - and a series of PASS messages

description("Tests dynamic updates of the 'href' attribute of the SVGTRefElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var textElement1 = createSVGElement("text");
textElement1.setAttribute("id", "testFail");
textElement1.textContent = "Test failed";
defsElement.appendChild(textElement1);

var textElement2 = createSVGElement("text");
textElement2.setAttribute("id", "testPass");
textElement2.textContent = "Test passed";
defsElement.appendChild(textElement2);
 
var textElement = createSVGElement("text");
textElement.setAttribute("y", "15");
rootSVGElement.appendChild(textElement);

var trefElement = createSVGElement("tref");
trefElement.setAttributeNS(xlinkNS, "xlink:href", "#testFail");
textElement.appendChild(trefElement);    

shouldBeEqualToString("trefElement.getAttributeNS('" + xlinkNS + "', 'href')", "#testFail");
if (window.internals) {
    var shadowRoot = internals.shadowRoot(trefElement);
    shouldBeEqualToString("shadowRoot.textContent", "Test failed");
}

function repaintTest() {
    trefElement.setAttributeNS(xlinkNS, "xlink:href", "#testPass");
    shouldBeEqualToString("trefElement.getAttributeNS('" + xlinkNS + "', 'href')", "#testPass");
    if (window.internals) {
        var shadowRoot = internals.shadowRoot(trefElement);
        shouldBeEqualToString("shadowRoot.textContent", "Test passed");
    }

    completeTest();
}

var successfullyParsed = true;
