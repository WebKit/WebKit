// [Name] option-element-onpick-recursion.js

createStaticWMLTestCase("Tests onpick intrinsic event recursion bug", "resources/option-element-onpick-recursion.wml", false);

var resultElement;

function setupTestDocument() {
    resultElement = testDocument.getElementById("result");
}

function prepareTest() {
    if (resultElement.textContent != "finished")
        window.setTimeout('startTest(25, 15)', 0);
    else
        completeTest();
}

function executeTest() {
    window.history.back();
}

var successfullyParsed = true;
