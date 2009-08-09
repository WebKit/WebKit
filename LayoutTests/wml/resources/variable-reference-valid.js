// [Name] variable-reference-valid.js 

createDynamicWMLTestCase("Tests valid variable references");

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

    // 'var2' doesn't exist yet, so the value will be empty
    var setvarElement2 = createWMLElement("setvar");
    setvarElement2.setAttribute("name", "wontwork");
    setvarElement2.setAttribute("value", "$var2");
    refreshElement.appendChild(setvarElement2);

    // 'var' already exists, so the value will be "TEST%20PASSED"
    var setvarElement3 = createWMLElement("setvar");
    setvarElement3.setAttribute("name", "var2");
    setvarElement3.setAttribute("value", "$(var:escape)");
    refreshElement.appendChild(setvarElement3);

    var setvarElement4 = createWMLElement("setvar");
    setvarElement4.setAttribute("name", "v");
    setvarElement4.setAttribute("value", "TEST PASSED");
    refreshElement.appendChild(setvarElement4);

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

    pElement7 = createWMLElement("p");
    pElement7.textContent = "Result: $wontwork";
    cardElement.appendChild(pElement7);
}

function prepareTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: $var");
    shouldBeEqualToString("pElement2.textContent", "Result: $(var:e)");
    shouldBeEqualToString("pElement3.textContent", "Result: $(var2)");
    shouldBeEqualToString("pElement4.textContent", "Result: $(var2:unesc)");
    shouldBeEqualToString("pElement5.textContent", "Result: $v");
    shouldBeEqualToString("pElement6.textContent", "Result: $(v:e)");
    shouldBeEqualToString("pElement7.textContent", "Result: $wontwork");

    startTest(25, 15);
}

function executeTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement2.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement3.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement4.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement5.textContent", "Result: TEST PASSED");
    shouldBeEqualToString("pElement6.textContent", "Result: TEST%20PASSED");
    shouldBeEqualToString("pElement7.textContent", "Result: ");

    completeTest();
}

var successfullyParsed = true;
