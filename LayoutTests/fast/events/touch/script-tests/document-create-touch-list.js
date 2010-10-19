description("This tests support for the document.createTouchList API.");

shouldBeTrue('"createTouchList" in document');

var touchList = document.createTouchList();
shouldBeNonNull("touchList");
shouldBe("touchList.length", "0");
shouldBeNull("touchList.item(0)");
shouldBeNull("touchList.item(1)");

successfullyParsed = true;
isSuccessfullyParsed();
