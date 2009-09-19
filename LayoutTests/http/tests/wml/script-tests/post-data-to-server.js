// [Name] post-data-to-server.js

createDynamicWMLTestCase("Complex data posting example", false);

var categorySelect;
var descInput;
var titleInput;
var urlInput;

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

    categorySelect = createWMLElement("select");
    categorySelect.setAttribute("name", "category");
    categorySelect.setAttribute("value", "wap");
    categorySelect.setAttribute("title", "Section?");
    categorySelect.setAttribute("multiple", "yes");

    var optionElement1 = createWMLElement("option");
    optionElement1.textContent = "Computers";
    optionElement1.setAttribute("value", "computers");
    categorySelect.appendChild(optionElement1);

    var optionElement2 = createWMLElement("option");
    optionElement2.textContent = "WAP";
    optionElement2.setAttribute("value", "wap");
    categorySelect.appendChild(optionElement2);

    var optionElement3 = createWMLElement("option");
    optionElement3.textContent = "Gateways";
    optionElement3.setAttribute("value", "gateways");
    categorySelect.appendChild(optionElement3);

    fieldsetElement1.appendChild(categorySelect);
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
    categorySelect.focus();

    testDocument.getElementById("top").focus();
    startTest(25, 15);
}

function executeTest() {
    completeTest();
}

var successfullyParsed = true;
