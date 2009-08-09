// [Name] go-task-post-method.js

createDynamicWMLTestCase("Tests POST method of &lt;go&gt; element", false);

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.textContent = "Start test";
    cardElement.appendChild(anchorElement);

    var goElement = createWMLElement("go");
    goElement.setAttribute("method", "POST");
    goElement.setAttribute("href", "http://127.0.0.1:8000/wml/resources/answer.cgi");

    var postfieldElement1 = createWMLElement("postfield");
    postfieldElement1.setAttribute("name", "var1");
    postfieldElement1.setAttribute("value", "Text with whitespace");
    goElement.appendChild(postfieldElement1);

    var postfieldElement2 = createWMLElement("postfield");
    postfieldElement2.setAttribute("name", "var2");
    postfieldElement2.setAttribute("value", "Text with entities: &auml; and &ouml;");
    goElement.appendChild(postfieldElement2);

    anchorElement.appendChild(goElement);
}

function prepareTest() {
    startTest(25, 15);
}

function executeTest() {
    completeTest();
}

var successfullyParsed = true;
