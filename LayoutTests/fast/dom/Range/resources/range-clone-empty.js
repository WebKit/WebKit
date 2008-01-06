description(
"This test checks cloning an empty range returns an empty fragment."
);

var r = document.createRange();
shouldBeTrue("r.cloneContents() != undefined");
shouldBeTrue("r.cloneContents() != null");

var successfullyParsed = true;
