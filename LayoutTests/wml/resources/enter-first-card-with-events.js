/// [Name] enter-first-card-with-events.js

createStaticWMLTestCase("Tests entering first card backwards that has intrinsic events set", "resources/enter-first-card-with-events.wml");

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    window.setTimeout('startTest(25, 15)', 0);
}

function executeTest() {
    var resultIndicatorElement = testDocument.getElementById("resultIndicator");
    if (resultIndicatorElement.textContent == "DONE")
        completeTest();
    else
        window.setTimeout('startTest(25, 15)', 0);
}

var successfullyParsed = true;
