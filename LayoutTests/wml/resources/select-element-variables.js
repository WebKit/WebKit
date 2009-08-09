// [Name] select-element-variables.js

createDynamicWMLTestCase("Tests variable references in conjuction with select elements");

var resultElement;

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;
    var card2Element = testDocument.getElementById("card2");

    var anchorElement = createWMLElement("anchor");
    anchorElement.textContent = "Start test";
    cardElement.appendChild(anchorElement);

    var goElement = createWMLElement("go");
    goElement.setAttribute("href", "#card2");
    anchorElement.appendChild(goElement);

    var pElement = createWMLElement("p");
    pElement.textContent = "Select an option";
    cardElement.appendChild(pElement);

    var selectElement = createWMLElement("select");
    selectElement.setAttribute("name", "result");
    cardElement.appendChild(selectElement);

    resultElement = card2Element.firstChild;
    resultElement.textContent = "Selection result: '$result'";

    var optionElement1 = createWMLElement("option");
    optionElement1.setAttribute("value", "doggy");
    optionElement1.textContent = "Dog";
    selectElement.appendChild(optionElement1);

    var optionElement2 = createWMLElement("option");
    optionElement2.setAttribute("value", "kitten");
    optionElement2.textContent = "Cat";
    selectElement.appendChild(optionElement2);
}

function prepareTest() {
    shouldBeEqualToString("resultElement.textContent", "Selection result: ''");
    startTest(25, 15);
}

function executeTest() {
    shouldBeEqualToString("resultElement.textContent", "Selection result: 'doggy'");
    completeTest();
}

var successfullyParsed = true;
