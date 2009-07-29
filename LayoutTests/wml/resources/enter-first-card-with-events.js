/// [Name] enter-first-card-with-events.js

createWMLTestCase("Tests entering first card backwards that has intrinsic events set", false, "resources/enter-first-card-with-events.wml", false);

function setupTestDocument() {
    var resultIndicatorElement = testDocument.getElementById("resultIndicator");
    if (resultIndicatorElement.textContent == "DONE")
        completeTest();
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    startTest(25, 15);

    // Force re-setup of the test document, as we check for completion upon setupTestDocument()
    testDocument = undefined;
}

var successfullyParsed = true;
