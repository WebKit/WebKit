// [Name] post-data-to-server.js

createWMLTestCase("Complex data posting example", false);
    
var titleInput;
var urlInput;
var descInput;

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.setAttribute("id", "top");
    anchorElement.textContent = "Start test";
    cardElement.appendChild(anchorElement);

    // Pure WML based form data posting using variable substitution
    var goElement = createWMLElement("go");
    goElement.setAttribute("method", "POST");
    goElement.setAttribute("href", "http://127.0.0.1:8000/wml/resources/dumpVariables.cgi");

    var postfieldElement1 = createWMLElement("postfield");
    postfieldElement1.setAttribute("name", "category");
    postfieldElement1.setAttribute("value", "$category");
    goElement.appendChild(postfieldElement1);

    var postfieldElement2 = createWMLElement("postfield");
    postfieldElement2.setAttribute("name", "title");
    postfieldElement2.setAttribute("value", "$title");
    goElement.appendChild(postfieldElement2);

    var postfieldElement3 = createWMLElement("postfield");
    postfieldElement3.setAttribute("name", "url");
    postfieldElement3.setAttribute("value", "$url");
    goElement.appendChild(postfieldElement3);

    var postfieldElement4 = createWMLElement("postfield");
    postfieldElement4.setAttribute("name", "desc");
    postfieldElement4.setAttribute("value", "$desc");
    goElement.appendChild(postfieldElement4);

    anchorElement.appendChild(goElement);

    // Category
    var fieldsetElement1 = createWMLElement("fieldset");
    fieldsetElement1.textContent = "Category:";

    var selectElement = createWMLElement("select");
    selectElement.setAttribute("name", "category");
    selectElement.setAttribute("value", "wap");
    selectElement.setAttribute("title", "Section?");

    var optionElement1 = createWMLElement("option");
    optionElement1.textContent = "Computers";
    optionElement1.setAttribute("value", "computers");
    selectElement.appendChild(optionElement1);

    var optionElement2 = createWMLElement("option");
    optionElement2.textContent = "WAP";
    optionElement2.setAttribute("value", "wap");
    selectElement.appendChild(optionElement2);

    var optionElement3 = createWMLElement("option");
    optionElement3.textContent = "Gateways";
    optionElement3.setAttribute("value", "gateways");
    selectElement.appendChild(optionElement3);

    fieldsetElement1.appendChild(selectElement);
    cardElement.appendChild(fieldsetElement1);

    // Title
    var fieldsetElement2 = createWMLElement("fieldset");
    fieldsetElement2.textContent = "Title:";

    titleInput = createWMLElement("input");
    titleInput.setAttribute("type", "text");
    titleInput.setAttribute("name", "title");
    titleInput.setAttribute("size", "20");
    titleInput.setAttribute("emptyok", "false");
    fieldsetElement2.appendChild(titleInput);
    cardElement.appendChild(fieldsetElement2);

    // URL
    var fieldsetElement3 = createWMLElement("fieldset");
    fieldsetElement3.textContent = "URL:";

    urlInput = createWMLElement("input");
    urlInput.setAttribute("type", "text");
    urlInput.setAttribute("name", "url");
    urlInput.setAttribute("size", "100");
    urlInput.setAttribute("emptyok", "false");
    fieldsetElement3.appendChild(urlInput);
    cardElement.appendChild(fieldsetElement3);

    // Description
    var fieldsetElement4 = createWMLElement("fieldset");
    fieldsetElement4.textContent = "Description:";

    descInput = createWMLElement("input");
    descInput.setAttribute("type", "text");
    descInput.setAttribute("name", "desc");
    descInput.setAttribute("size", "800");
    descInput.setAttribute("emptyok", "true");
    fieldsetElement4.appendChild(descInput);
    cardElement.appendChild(fieldsetElement4);
}

function sendTextToControl(element, text) {
    element.focus();
    testDocument.execCommand("InsertText", false, text);
}

function prepareTest() {
    sendTextToControl(titleInput, "[title] TEST PASSED");
    sendTextToControl(urlInput, "[url] TEST PASSED");
    sendTextToControl(descInput, "[desc] TEST PASSED");

    testDocument.getElementById("top").focus();
    startTest(25, 15);
}

function executeTest() {
    completeTest();
}

var successfullyParsed = true;
