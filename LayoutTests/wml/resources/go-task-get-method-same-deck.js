// [Name] go-task-get-method-same-deck.js

createWMLTestCase("Tests GET method of <go> element - jump within deck", false);

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.appendChild(testDocument.createTextNode("Start test"));
    cardElement.appendChild(anchorElement);

    var goElement = createWMLElement("go");
    goElement.setAttribute("href", "#card2");
    anchorElement.appendChild(goElement);

    var targetCardElement = createWMLElement("card");
    targetCardElement.setAttribute("id", "card2");
    cardElement.parentNode.appendChild(targetCardElement);

    var pElement = createWMLElement("p");
    pElement.textContent = "Test passed";
    targetCardElement.appendChild(pElement);
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    completeTest();
}

var successfullyParsed = true;
