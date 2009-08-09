// [Name] variable-reference-invalid-character.js 

createDynamicWMLTestCase("Test that invalid variable references aren't detected until variable substitution is executed");

var pElement1;
var pElement2;
var pElement3;
var pElement4;

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.textContent = "Start test";
    cardElement.appendChild(anchorElement);

    var refreshElement = createWMLElement("refresh");
    anchorElement.appendChild(refreshElement);

    var setvarElement = createWMLElement("setvar");
    setvarElement.setAttribute("name", "var");
    setvarElement.setAttribute("value", "TEST PASSED");
    refreshElement.appendChild(setvarElement);

    pElement1 = createWMLElement("p");
    pElement1.textContent = "Result:$#var PASSED";
    cardElement.appendChild(pElement1);

    pElement2 = createWMLElement("p");
    pElement2.textContent = "Result: $(v#ar:e)PASSED";
    cardElement.appendChild(pElement2);

    pElement3 = createWMLElement("p");
    pElement3.textContent = "Result:$(var:#) PASSED";
    cardElement.appendChild(pElement3);

    pElement4 = createWMLElement("p");
    pElement4.textContent = "Result: PASSED$(var:unescape)";
    cardElement.appendChild(pElement4);
}

function prepareTest() {
    shouldBeEqualToString("pElement1.textContent", "Result:$#var PASSED");
    shouldBeEqualToString("pElement2.textContent", "Result: $(v#ar:e)PASSED");
    shouldBeEqualToString("pElement3.textContent", "Result:$(var:#) PASSED");
    shouldBeEqualToString("pElement4.textContent", "Result: PASSED$(var:unescape)");
 
    startTest(25, 15);
}

function executeTest() {
    // No variables should have been substituted, instead four console error messages
    // should be included in the expected layout test results
    shouldBeEqualToString("pElement1.textContent", "Result:$#var PASSED");
    shouldBeEqualToString("pElement2.textContent", "Result: $(v#ar:e)PASSED");
    shouldBeEqualToString("pElement3.textContent", "Result:$(var:#) PASSED");
    shouldBeEqualToString("pElement4.textContent", "Result: PASSED$(var:unescape)");

    completeTest();
}

var successfullyParsed = true;
