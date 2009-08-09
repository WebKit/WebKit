// [Name] go-task-get-method-accept-charset.js

createDynamicWMLTestCase("Tests GET method of &lt;go&gt; element - accept-charset attribute", false);

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.textContent = "Start test";
    cardElement.appendChild(anchorElement);

    var goElement = createWMLElement("go");
    goElement.setAttribute("method", "GET");
    goElement.setAttribute("accept-charset", "utf-8");
    goElement.setAttribute("href", "http://127.0.0.1:8000/wml/resources/answer-utf8.cgi");

    var postfieldElement1 = createWMLElement("postfield");
    postfieldElement1.setAttribute("name", "var1");
    postfieldElement1.setAttribute("value", "Text with whitespace");
    goElement.appendChild(postfieldElement1);

    var postfieldElement2 = createWMLElement("postfield");
    postfieldElement2.setAttribute("name", "var2");
    postfieldElement2.setAttribute("value", "Text with non-latin1 characters: Schön, daß es Ä&Ouml;Ü gibt!");
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
