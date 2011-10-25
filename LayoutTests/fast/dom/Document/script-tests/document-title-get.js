description("This test checks the implementation of getting the document.title attribute.");

debug('Test with no title set');
shouldBeEqualToString("document.title", "");

debug('Test with empty title');
document.title = "";
shouldBeEqualToString("document.title","");

debug('Test with only whitespace');
document.title = "\t\n\r    \r     \t\n\n";
shouldBeEqualToString("document.title","");

debug('Test with no whitespace');
document.title = "nowhitespacetitle";
shouldBeEqualToString("document.title","nowhitespacetitle");

debug('Test with whitespace');
document.title = "\u0009\u000aone\u000b\u000cspace\u000d\u0020";
shouldBeEqualToString("document.title","one space");

debug('Test with various whitespace lengths and fields');
document.title = "   lots of \r whitespace and \n\n\n    \t    newlines \t";
shouldBeEqualToString("document.title", "lots of whitespace and newlines");

debug('Test with various length strings');
