description("This test checks to see if document.head is available, readonly, and the same element as what we would expect by getting it by other means.");

shouldBe("document.head", "document.getElementsByTagName('head')[0]");

document.head = 1;
shouldBeTrue("document.head !== 1");

document.documentElement.removeChild(document.head);
shouldBeNull("document.head");

var successfullyParsed = true;
