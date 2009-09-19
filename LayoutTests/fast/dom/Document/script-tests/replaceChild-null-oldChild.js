description('Test behavior of Document.replaceChild() when oldChild is null.');

shouldThrow('document.replaceChild(document.firstChild, null)', '"Error: NOT_FOUND_ERR: DOM Exception 8"');

var successfullyParsed = true;
