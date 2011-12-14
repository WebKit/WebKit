description(
"This tests that the querySelector and querySelectorAll fast path for IDs is not overzelous."
);

var root = document.createElement('div');
var correctNode = document.createElement('div');
correctNode.setAttribute("id", "testid")
root.appendChild(correctNode);
document.body.appendChild(root);

shouldBe("document.querySelector('div#testid')", "correctNode");
shouldBe("document.querySelector('#testid')", "correctNode");
shouldBeNull("document.querySelector('ul#testid')");
shouldBeNull("document.querySelector('ul #testid')");
shouldBeNull("document.querySelector('#testid[attr]')");
shouldBeNull("document.querySelector('#testid:not(div)')");

shouldBe("document.querySelectorAll('div#testid').length", "1");
shouldBe("document.querySelectorAll('div#testid').item(0)", "correctNode");
shouldBe("document.querySelectorAll('#testid').length", "1");
shouldBe("document.querySelectorAll('#testid').item(0)", "correctNode");
shouldBe("document.querySelectorAll('ul#testid').length", "0");
shouldBe("document.querySelectorAll('ul #testid').length", "0");
shouldBe("document.querySelectorAll('#testid[attr]').length", "0");
shouldBe("document.querySelectorAll('#testid:not(div)').length", "0");

var successfullyParsed = true;
