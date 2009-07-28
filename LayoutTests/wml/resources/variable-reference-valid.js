// [Name] variable-reference-valid.js 

createWMLTestCase("Tests valid variable references");

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.textContent = "Start test";
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

    var setvarElement3 = createWMLElement("setvar");
    setvarElement3.setAttribute("name", "v");
    setvarElement3.setAttribute("value", "TEST PASSED");
    refreshElement.appendChild(setvarElement3);

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

    pElement5 = createWMLElement("p");
    pElement5.textContent = "Result: $v";
    cardElement.appendChild(pElement5);

    pElement6 = createWMLElement("p");
    pElement6.textContent = "Result: $(v:e)";
    cardElement.appendChild(pElement6);
}

function prepareTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: $var");
    shouldBeEqualToString("pElement2.textContent", "Result: $(var:e)");
    shouldBeEqualToString("pElement3.textContent", "Result: $(var2)");
    shouldBeEqualToString("pElement4.textContent", "Result: $(var2:unesc)");
    shouldBeEqualToString("pElement5.textContent", "Result: $v");
    shouldBeEqualToString("pElement6.textContent", "Result: $(v:e)");

    startTest(25, 15);
}

function executeTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement2.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement3.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement4.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement5.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement6.textContent", "Result: TEST%20PASSED");

    completeTest();
}

var successfullyParsed = true;
