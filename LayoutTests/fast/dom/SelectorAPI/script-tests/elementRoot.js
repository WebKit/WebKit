description(
"This tests that querySelector and querySelectorAll don't search outside their root node."
);

var root = document.createElement('div');
var correctNode = document.createElement('div');
root.appendChild(correctNode);
document.body.appendChild(root);
var noChild = document.createElement('div');
document.body.appendChild(noChild);

shouldBe("root.querySelector('div')", "correctNode");
shouldBe("root.querySelectorAll('div').length", "1");
shouldBe("root.querySelectorAll('div').item(0)", "correctNode");

shouldBeNull("noChild.querySelector('div')");
shouldBe("noChild.querySelectorAll('div').length", "0");

var successfullyParsed = true;
