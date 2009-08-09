/// [Name] ontimer-event.js

createStaticWMLTestCase("Tests ontimer non-inline event declarations", "resources/ontimer-event.wml");

var counter = 0;

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    // no-op
}

function executeTest() {
    if (counter == 2)
        completeTest();

    ++counter;
}

var successfullyParsed = true;
