// [Name] variable-reference-valid.js 

createWMLTestCase("Tests valid variable references");

var pElement1;
var pElement2;
var pElement3;
var pElement4;

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.appendChild(testDocument.createTextNode("Start test"));
    cardElement.appendChild(anchorElement);

    var refreshElement = createWMLElement("refresh");
    anchorElement.appendChild(refreshElement);

    var setvarElement1 = createWMLElement("setvar");
    setvarElement1.setAttribute("name", "var");
    setvarElement1.setAttribute("value", "TEST PASSED");
    refreshElement.appendChild(setvarElement1);

    var setvarElement2 = createWMLElement("setvar");
    setvarElement2.setAttribute("name", "var2");

    // FIXME: Use $(var:escape) instead of TEST%20PASSED.
    // This doesn't work at the moment. Investigate.
    setvarElement2.setAttribute("value", "TEST%20PASSED");
    refreshElement.appendChild(setvarElement2);

    pElement1 = createWMLElement("p");
    pElement1.textContent = "Result: $var";
    cardElement.appendChild(pElement1);

    pElement2 = createWMLElement("p");
    pElement2.textContent = "Result: $(var:e)";
    cardElement.appendChild(pElement2);

    pElement3 = createWMLElement("p");
    pElement3.textContent = "Result: $(var2)";
    cardElement.appendChild(pElement3);

    pElement4 = createWMLElement("p");
    pElement4.textContent = "Result: $(var2:unesc)";
    cardElement.appendChild(pElement4);
}

function prepareTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: $var");
    shouldBeEqualToString("pElement2.textContent", "Result: $(var:e)");
    shouldBeEqualToString("pElement3.textContent", "Result: $(var2)");
    shouldBeEqualToString("pElement4.textContent", "Result: $(var2:unesc)");
 
    startTest(25, 15);
}

function executeTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement2.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement3.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement4.textContent", "Result: TEST PASSED");

    completeTest();
}
