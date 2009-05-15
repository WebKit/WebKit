description('This tests pasting a singly nested blockquote before, and outside of, another blockquote.  This test does not work outside of DRT.');

var createLine = function(num, parent) {
  var line = document.createElement('div');
  line.innerHTML = 'line ' + num;
  parent.appendChild(line);
  return line;
}

var pasteDiv = document.createElement("div");
pasteDiv.id = "pasteDiv";
pasteDiv.contentEditable = true;
document.body.appendChild(pasteDiv);

var outerBlockquote = document.createElement('blockquote');
outerBlockquote.type = 'cite';

createLine(1, outerBlockquote);
var line2 = createLine(2, outerBlockquote);
line2.id = 'singleNestedNodeToCopy';
createLine(3, outerBlockquote);

var innerBlockquote = document.createElement('blockquote');
innerBlockquote.type = 'cite';
createLine(4, innerBlockquote);
outerBlockquote.appendChild(innerBlockquote);

createLine(5, outerBlockquote);

var div = document.createElement("div");
div.appendChild(outerBlockquote);
div.appendChild(document.createElement('br'));
document.body.appendChild(div);

function copyAndPasteNode(nodeName) {
    var range = document.createRange();

    // Test with a single blockquote
    var nodeToCopy = document.getElementById(nodeName);
    range.setStartBefore(nodeToCopy);
    range.setEndAfter(nodeToCopy);

    var sel = window.getSelection();
    sel.addRange(range);
    document.execCommand("Copy");

    var pasteDiv = document.getElementById("pasteDiv");
    sel.setPosition(pasteDiv, 0);
    document.execCommand("Paste");

    if (pasteDiv.children[1] == undefined)
        testFailed("Pasting " + nodeName + " DID keep the newline within the blockquote");
    else
        testPassed("Pasting " + nodeName + " DID NOT keep the newline within the blockquote.");
}

copyAndPasteNode('singleNestedNodeToCopy');

var successfullyParsed = true;
