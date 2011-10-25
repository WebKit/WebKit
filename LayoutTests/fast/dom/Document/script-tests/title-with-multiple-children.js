description("This test checks to see if setting document.title works even if the title element has multiple children.");

// Setup - create title element.
shouldBe("document.getElementsByTagName('title').length", "0");
var titleElement = document.createElement("title");
document.body.appendChild(titleElement);

// For case with no children.
shouldBe("document.title", "''");
shouldBe("titleElement.text", "''");

// For case with single children.
var firstText = "First";
titleElement.appendChild(document.createTextNode(firstText));
shouldBe("document.title", "firstText");
shouldBe("titleElement.text", "firstText");

// For case with 2 children.
var secondText = "Second";
titleElement.appendChild(document.createTextNode(secondText));
shouldBe("document.title", "firstText + secondText");
shouldBe("titleElement.text", "firstText + secondText");

// override title with setting document.title with multiple title children.
var expected = "This title is set by property";
document.title = expected;
shouldBe("document.title", "expected");
shouldBe("titleElement.text", "expected");
