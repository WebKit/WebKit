description("Test that collapseToStart() and collapseToEnd() throw INVALID_STATE_ERR if no selection is made.");

var sel = window.getSelection();
var textNode = document.createTextNode("abcdef");
document.body.appendChild(textNode);

shouldThrow("sel.collapseToStart()", "'Error: INVALID_STATE_ERR: DOM Exception 11'");
shouldThrow("sel.collapseToEnd()", "'Error: INVALID_STATE_ERR: DOM Exception 11'");

sel.selectAllChildren(textNode);

shouldBe("sel.collapseToStart()", "undefined");
shouldBe("sel.collapseToEnd()", "undefined");

document.body.removeChild(textNode);

var successfullyParsed = true;
