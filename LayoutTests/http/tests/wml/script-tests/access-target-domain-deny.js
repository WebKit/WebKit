// [Name] access-target-domain-deny.js

createStaticWMLTestCase("Tests access control support", "resources/access-target-domain-deny.wml");

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    var lastError = console.lastWMLErrorMessage();
    if (lastError == "Deck not accessible.") {
        completeTest();
        return;
    }

    startTest(25, 15);
}

function executeTest() {
    // no-op
}

var successfullyParsed = true;
