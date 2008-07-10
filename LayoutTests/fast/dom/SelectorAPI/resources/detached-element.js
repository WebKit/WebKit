description(
"This tests that querySelector and querySelectorAll work with elements that are not in a document yet."
);

var root = document.createElement('div');
var correctNode = document.createElement('div');
correctNode.setAttribute("id", "testId")
root.appendChild(correctNode);
var noChild = document.createElement('div');

shouldBe("root.querySelector('div')", "correctNode");
shouldBe("root.querySelector('#testId')", "correctNode");

shouldBe("root.querySelectorAll('div').length", "1");
shouldBe("root.querySelectorAll('div').item(0)", "correctNode");
shouldBe("root.querySelectorAll('#testId').length", "1");
shouldBe("root.querySelectorAll('#testId').item(0)", "correctNode");

shouldBeNull("noChild.querySelector('div')");
shouldBe("noChild.querySelectorAll('div').length", "0");

var successfullyParsed = true;
