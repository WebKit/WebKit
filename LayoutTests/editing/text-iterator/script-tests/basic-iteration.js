description('Unit tests for WebCore text iterator');

var subframe = document.createElement('iframe');
document.body.appendChild(subframe);

var testDocument = subframe.contentDocument;
var range = testDocument.createRange();

var head = testDocument.createElement("head");
testDocument.documentElement.insertBefore(head, testDocument.documentElement.firstChild);

testDocument.body.innerHTML = '';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '""');

testDocument.body.innerHTML = 'a';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '"a"');

testDocument.body.innerHTML = '<div>a</div>';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '"a"');

testDocument.body.innerHTML = '<div>a</div><div>b</div>';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '"a\\nb"');

testDocument.body.innerHTML = '<div style="line-height: 18px; min-height: 436px; " id="node-content" class="note-content">debugging this note</div>';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '"debugging this note"');

testDocument.body.innerHTML = '<div>Hello<div><span><span><br></div></div>';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '"Hello\\n"');

testDocument.body.innerHTML = '<div class="note-rule-vertical" style="left:22px"></div>\n\t\t<div class="note-rule-vertical" style="left:26px"></div>\n\n\t\t<div class="note-wrapper">\n\t\t\t<div class="note-header">\n\t\t\t\t<div class="note-body" id="note-body">\n\t\t\t\t\t<div class="note-content" id="note-content" contenteditable="true" style="line-height: 20px; min-height: 580px; ">hey</div>\n\t\t\t\t</div>\n\t\t\t</div>\n\t\t</div>\n\t\n';
shouldBe('range.selectNodeContents(testDocument.body); plainText.plainText(range)', '"hey"');
shouldBe('range.setStartBefore(testDocument.body); range.setEndAfter(testDocument.body); plainText.plainText(range)', '"hey"');

document.body.removeChild(subframe);

var successfullyParsed = true;
