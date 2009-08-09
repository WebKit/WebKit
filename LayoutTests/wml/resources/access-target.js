// [Name] access-target.js

createStaticWMLTestCase("Tests access control support", "resources/access-target.wml");

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    var resultIndicatorElement = testDocument.getElementById("resultIndicator");
    if (resultIndicatorElement)
        completeTest();
}

var successfullyParsed = true;
