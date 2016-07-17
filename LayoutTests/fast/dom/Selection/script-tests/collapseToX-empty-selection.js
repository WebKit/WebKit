description("Test that collapseToStart() and collapseToEnd() throw INVALID_STATE_ERR if no selection is made.");

var sel = window.getSelection();
var textNode = document.createTextNode("abcdef");
document.body.appendChild(textNode);

shouldThrow("sel.collapseToStart()", "'InvalidStateError (DOM Exception 11): The object is in an invalid state.'");
shouldThrow("sel.collapseToEnd()", "'InvalidStateError (DOM Exception 11): The object is in an invalid state.'");

sel.selectAllChildren(textNode);

shouldBe("sel.collapseToStart()", "undefined");
shouldBe("sel.collapseToEnd()", "undefined");

document.body.removeChild(textNode);
