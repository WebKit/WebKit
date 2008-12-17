/// [Name] go-task-animation.js

createWMLTestCase("Tests &lt;timer&gt; and &lt;go&gt; element combinations", false, "resources/animation.wml");

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
