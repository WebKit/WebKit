/// [Name] newcontext-same-deck.js

createStaticWMLTestCase("Tests newcontext attribute handling on cards within the same deck", "resources/newcontext-same-deck.wml");

var counter = 0;

var result1;
var result2;
var result3;

function setupTestDocument() {
    // no-op
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    result1 = testDocument.getElementById("result1");
    result2 = testDocument.getElementById("result2");
    result3 = testDocument.getElementById("result3");

    if (counter == 0) {
        shouldBeEqualToString("result1.textContent", "Test 1/3: var1=''");
        startTest(25, 15);
    } else if (counter == 1) {
        shouldBeEqualToString("result2.textContent", "Test 2/3: var1=''");
        shouldBeEqualToString("result3.textContent", "Test 3/3: var2='PASS'");
        completeTest();
    }

    ++counter;
}

var successfullyParsed = true;
