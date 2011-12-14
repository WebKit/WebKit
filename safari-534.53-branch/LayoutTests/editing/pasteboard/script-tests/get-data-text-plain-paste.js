description('This tests that getData is supported for type text/plain during paste.  We select "foo", copy it, and then check that on paste getData returns "foo". To run manually, select "foo", copy it, and paste it.');

var handlePaste = function(e) {
  var pasteText = e.clipboardData.getData("text/plain");
  // Check that the content we get is the same content we copied.
  var expectedText = 'foo';
  if (pasteText == expectedText)
    testPassed("Pasted text matched expected text");
  else
    testFailed("Expected: " +expectedText + "\nbut was: " + pasteText);
}

var editDiv = document.createElement('div');
editDiv.contentEditable = true;
editDiv.innerHTML = "foo";
document.body.appendChild(editDiv);
editDiv.addEventListener('paste', handlePaste, false);
    
// Select foo and copy it.    
var selection = window.getSelection();
selection.setPosition(editDiv, 0);
selection.modify("extend", "forward", "word"); 
document.execCommand("Copy");

// Paste, and make sure we can read the html text off the clipboard.
document.execCommand("Paste");
         
var successfullyParsed = true;
