/// [Name] enter-card-with-events.js

createStaticWMLTestCase("Tests entering cards in forward and backward directions that have intrinsic events set", "resources/enter-card-with-events.wml");

var counter = 0;

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    if (counter == 3)
        completeTest();
    else if (counter == 1)
        startTest(25, 15);
        
    ++counter;
}

var successfullyParsed = true;
