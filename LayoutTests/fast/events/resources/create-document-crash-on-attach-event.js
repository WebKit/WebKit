description('This test checks for crashes in setting an event handler on a document element created by '
            + 'document.implementation.createDocument.');

var doc = document.implementation.createDocument('', '', null);
doc.onload = function() { };
testPassed('Attached onload event handler to created document.');

var successfullyParsed = true;
