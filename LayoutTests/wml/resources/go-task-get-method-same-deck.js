// [Name] go-task-get-method-same-deck.js

createWMLTestCase("Tests GET method of &lt;go&gt; element - jump within deck", false);

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.appendChild(testDocument.createTextNode("Start test"));
    cardElement.appendChild(anchorElement);

    var goElement = createWMLElement("go");
    goElement.setAttribute("href", "#card2");
    anchorElement.appendChild(goElement);
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    completeTest();
}

var successfullyParsed = true;
