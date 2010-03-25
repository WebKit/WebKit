description('This tests the fix for &lt;rdar://problem/6633727&gt; Hitting return at the end of a line with an anchor jumps me to the bottom of the message. If the test has passed, the numbers should be in order, and only "1" should be a link.');

// Set up the div
var editable = document.createElement('div');
editable.contentEditable = true;
editable.innerHTML = '<a href="#" id="anchor">1</a><div>3</div>';
document.body.appendChild(editable);
editable.focus();

// Edit it with execCommand
var sel = window.getSelection();
sel.setPosition(document.getElementById('anchor'), 1);
document.execCommand("InsertParagraph");
document.execCommand("InsertText", false, "2");

shouldBe("editable.innerHTML", "'"+'<a href="#" id="anchor">1</a><div><a href="#" id="anchor"></a>2<br><div>3</div></div>'+"'");

shouldBe("sel.baseNode", "editable.childNodes.item(1).childNodes.item(1)");
shouldBe("sel.baseNode.nodeType", "Node.TEXT_NODE");
shouldBe("sel.baseOffset", "1");

var successfullyParsed = true;
