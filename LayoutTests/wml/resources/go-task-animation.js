/// [Name] go-task-animation.js

createWMLTestCase("Tests <timer> and <go> element combinations", false, "resources/animation.wml");

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
