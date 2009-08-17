// [Name] option-element-onpick.js

createDynamicWMLTestCase("Tests onpick intrinsic event support of option elements", false);

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var selectElement = createWMLElement("select");
    selectElement.setAttribute("name", "result");
    selectElement.setAttribute("multiple", "false");
    cardElement.appendChild(selectElement);

    var optionElement1 = createWMLElement("option");
    optionElement1.setAttribute("onpick", "#card2");
    optionElement1.setAttribute("value", "doggy");
    optionElement1.textContent = "Dog";
    selectElement.appendChild(optionElement1);

    var optionElement2 = createWMLElement("option");
    optionElement2.setAttribute("value", "kitten");
    optionElement2.textContent = "Cat";
    selectElement.appendChild(optionElement2);
}

function prepareTest() {
    window.setTimeout('startTest(25, 15)', 0);
}

function executeTest() {
    completeTest();
}

var successfullyParsed = true;
