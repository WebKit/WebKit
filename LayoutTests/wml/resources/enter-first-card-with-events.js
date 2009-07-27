/// [Name] enter-first-card-with-events.js

createWMLTestCase("Tests entering first card backwards that has intrinsic events set", false, "resources/enter-first-card-with-events.wml", false, false);

var ranOnce = false;
var resultIndicatorElement;

function setupTestDocument() {
    resultIndicatorElement = testDocument.getElementById("resultIndicator");
}

function prepareTest() {
    if (resultIndicatorElement.textContent == "DONE")
        completeTest();
    else
        executeTest();
}

function executeTest() {
    startTest(25, 15);
}

var successfullyParsed = true;
