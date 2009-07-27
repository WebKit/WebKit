/// [Name] enter-card-with-events.js

createWMLTestCase("Tests entering cards in forward and backward directions that have intrinsic events set", false, "resources/enter-card-with-events.wml", false);

var ranOnce = false;

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    if (ranOnce)
        completeTest();
    else {
        ranOnce = true;
        startTest(25, 15);
    }
}

var successfullyParsed = true;
