// [Name] input-format.js 

createDynamicWMLTestCase("Tests input format validation");

var pElement1;
var pElement2;
var pElement3;
var pElement4;
var pElement5;
var pElement6;
var pElement7;
var pElement8;
var pElement9;
var pElement10;
var pElement11;
var pElement12;

function setupTestDocument() {
    var cardElement = testDocument.documentElement.firstChild;

    var anchorElement = createWMLElement("anchor");
    anchorElement.textContent = "Start test";
    cardElement.appendChild(anchorElement);

    var refreshElement = createWMLElement("refresh");
    anchorElement.appendChild(refreshElement);

    // Test cases for successful matching, the 'value' can display correclty
    var inputElement1 = createWMLElement("input");
    inputElement1.setAttribute("name", "input1");
    inputElement1.setAttribute("value", "TeSt0-01");
    inputElement1.setAttribute("format", "AaXxMmNn");
    cardElement.appendChild(inputElement1);

    var inputElement2 = createWMLElement("input");
    inputElement2.setAttribute("name", "input2");
    inputElement2.setAttribute("value", "TeSt0-02");
    inputElement2.setAttribute("format", "AaXxn\-2N");
    cardElement.appendChild(inputElement2);

    var inputElement3 = createWMLElement("input");
    inputElement3.setAttribute("name", "input3");
    inputElement3.setAttribute("value", "TeSt-+03");
    inputElement3.setAttribute("format", "AaXx\-\+nN");
    cardElement.appendChild(inputElement3);

    var inputElement4 = createWMLElement("input");
    inputElement4.setAttribute("name", "input4");
    inputElement4.setAttribute("value", "TeSt0-04");
    inputElement4.setAttribute("format", "*m");
    cardElement.appendChild(inputElement4);

    var inputElement5 = createWMLElement("input");
    inputElement5.setAttribute("name", "input5");
    inputElement5.setAttribute("value", "TeSt0-05");
    inputElement5.setAttribute("format", "Xam*x");
    cardElement.appendChild(inputElement5);

    // Test cases for failed matching, if failed, the 'value' should be set to empty
    var inputElement6 = createWMLElement("input");
    inputElement6.setAttribute("name", "input6");
    inputElement6.setAttribute("value", "TeSt0-06");
    inputElement6.setAttribute("format", "MaXxa3n");
    cardElement.appendChild(inputElement6);
 
    var inputElement7 = createWMLElement("input");
    inputElement7.setAttribute("name", "input7");
    inputElement7.setAttribute("value", "TeSt0-07");
    inputElement7.setAttribute("format", "aaxxmmnn");
    cardElement.appendChild(inputElement7);

    // Test cases for invalid input mask which should be ignored, 
    // and the 'value' can display correclty
    var inputElement8 = createWMLElement("input");
    inputElement8.setAttribute("name", "input8");
    inputElement8.setAttribute("value", "TeSt0-08");
    inputElement8.setAttribute("format", "TeSt0008");
    cardElement.appendChild(inputElement8);

    var inputElement9 = createWMLElement("input");
    inputElement9.setAttribute("name", "input9");
    inputElement9.setAttribute("value", "TeSt0-09");
    inputElement9.setAttribute("format", "MMMMMMMM0n");
    cardElement.appendChild(inputElement9);

    var inputElement10 = createWMLElement("input");
    inputElement10.setAttribute("name", "input10");
    inputElement10.setAttribute("value", "0123456789");
    inputElement10.setAttribute("format", "10n");
    cardElement.appendChild(inputElement10);

    var inputElement11 = createWMLElement("input");
    inputElement11.setAttribute("name", "input11");
    inputElement11.setAttribute("value", "TeSt0-11");
    inputElement11.setAttribute("format", "*AMMMMMM");
    cardElement.appendChild(inputElement11);

    var inputElement12= createWMLElement("input");
    inputElement12.setAttribute("name", "input12");
    inputElement12.setAttribute("value", "TeSt0-12");
    inputElement12.setAttribute("format", "M*a*M");
    cardElement.appendChild(inputElement12);

    pElement1 = createWMLElement("p");
    pElement1.textContent = "Result: $input1";
    cardElement.appendChild(pElement1);

    pElement2 = createWMLElement("p");
    pElement2.textContent = "Result: $input2";
    cardElement.appendChild(pElement2);

    pElement3 = createWMLElement("p");
    pElement3.textContent = "Result: $input3";
    cardElement.appendChild(pElement3);

    pElement4 = createWMLElement("p");
    pElement4.textContent = "Result: $input4";
    cardElement.appendChild(pElement4);

    pElement5 = createWMLElement("p");
    pElement5.textContent = "Result: $input5";
    cardElement.appendChild(pElement5);

    pElement6 = createWMLElement("p");
    pElement6.textContent = "Result: $input6";
    cardElement.appendChild(pElement6);

    pElement7 = createWMLElement("p");
    pElement7.textContent = "Result: $input7";
    cardElement.appendChild(pElement7);

    pElement8 = createWMLElement("p");
    pElement8.textContent = "Result: $input8";
    cardElement.appendChild(pElement8);

    pElement9 = createWMLElement("p");
    pElement9.textContent = "Result: $input9";
    cardElement.appendChild(pElement9);

    pElement10 = createWMLElement("p");
    pElement10.textContent = "Result: $input10";
    cardElement.appendChild(pElement10);

    pElement11 = createWMLElement("p");
    pElement11.textContent = "Result: $input11";
    cardElement.appendChild(pElement11);

    pElement12 = createWMLElement("p");
    pElement12.textContent = "Result: $input12";
    cardElement.appendChild(pElement12);
}

function prepareTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: $input1");
    shouldBeEqualToString("pElement2.textContent", "Result: $input2");
    shouldBeEqualToString("pElement3.textContent", "Result: $input3");
    shouldBeEqualToString("pElement4.textContent", "Result: $input4");
    shouldBeEqualToString("pElement5.textContent", "Result: $input5");
    shouldBeEqualToString("pElement6.textContent", "Result: $input6");
    shouldBeEqualToString("pElement7.textContent", "Result: $input7");
    shouldBeEqualToString("pElement8.textContent", "Result: $input8");
    shouldBeEqualToString("pElement9.textContent", "Result: $input9");
    shouldBeEqualToString("pElement10.textContent", "Result: $input10");
    shouldBeEqualToString("pElement11.textContent", "Result: $input11");
    shouldBeEqualToString("pElement12.textContent", "Result: $input12");

    startTest(25, 15);
}

function executeTest() {
    shouldBeEqualToString("pElement1.textContent", "Result: TeSt0-01");
    shouldBeEqualToString("pElement2.textContent", "Result: TeSt0-02");
    shouldBeEqualToString("pElement3.textContent", "Result: TeSt-+03");
    shouldBeEqualToString("pElement4.textContent", "Result: TeSt0-04");
    shouldBeEqualToString("pElement5.textContent", "Result: TeSt0-05");
    shouldBeEqualToString("pElement6.textContent", "Result: ");
    shouldBeEqualToString("pElement7.textContent", "Result: ");
    shouldBeEqualToString("pElement8.textContent", "Result: TeSt0-08");
    shouldBeEqualToString("pElement9.textContent", "Result: TeSt0-09");
    shouldBeEqualToString("pElement10.textContent", "Result: 0123456789");
    shouldBeEqualToString("pElement11.textContent", "Result: TeSt0-11");
    shouldBeEqualToString("pElement12.textContent", "Result: TeSt0-12");

    completeTest();
}

var successfullyParsed = true;
