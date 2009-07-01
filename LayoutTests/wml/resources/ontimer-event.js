/// [Name] ontimer-event.js

createWMLTestCase("Tests ontimer non-inline event declarations", false, "resources/ontimer-event.wml");

var counter = 0;

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    // no-op
}

function executeTest() {
    if (counter == 3) {
        completeTest();
    }

    ++counter;
}

var successfullyParsed = true;
