description(
"This tests that the querySelector and querySelectorAll correctly stringify null and undefined to \"null\" and \"undefined\"."
);

var root = document.createElement('div');
var nullNode = document.createElement('null');
root.appendChild(nullNode);
var undefinedNode = document.createElement('undefined');
root.appendChild(undefinedNode);
document.body.appendChild(root);

shouldBe("document.querySelector(null)", "nullNode");
shouldBe("document.querySelector(undefined)", "undefinedNode");

shouldBe("document.querySelectorAll(null).length", "1");
shouldBe("document.querySelectorAll(null).item(0)", "nullNode");
shouldBe("document.querySelectorAll(undefined).length", "1");
shouldBe("document.querySelectorAll(undefined).item(0)", "undefinedNode");

var successfullyParsed = true;
