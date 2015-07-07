description(
"This tests that the querySelector and querySelectorAll fast path for IDs is not overzelous."
);

var root = document.createElement('div');
var correctNode = document.createElement('div');
correctNode.setAttribute("id", "testId")
root.appendChild(correctNode);
document.body.appendChild(root);


shouldBe("document.querySelector('#testId')", "correctNode");
shouldBe("document.querySelector('div#testId')", "correctNode");
shouldBeNull("document.querySelector('ul#testId')");
shouldBeNull("document.querySelector('ul #testId')");
shouldBeNull("document.querySelector('#testId[attr]')");
shouldBeNull("document.querySelector('#testId:not(div)')");

shouldBe("document.querySelectorAll('div#testId').length", "1");
shouldBe("document.querySelectorAll('div#testId').item(0)", "correctNode");
shouldBe("document.querySelectorAll('#testId').length", "1");
shouldBe("document.querySelectorAll('#testId').item(0)", "correctNode");
shouldBe("document.querySelectorAll('ul#testId').length", "0");
shouldBe("document.querySelectorAll('ul #testId').length", "0");
shouldBe("document.querySelectorAll('#testId[attr]').length", "0");
shouldBe("document.querySelectorAll('#testId:not(div)').length", "0");
