description(
'This test checks the scroll, client and offset dimension properties return "0" instead of undefined for an unrendered element. <div id="testDiv" style="display: none">x</div>'
);

var testDiv = document.getElementById("testDiv");
shouldBe("testDiv.offsetLeft", "0");
shouldBe("testDiv.offsetTop", "0");
shouldBe("testDiv.offsetWidth", "0");
shouldBe("testDiv.offsetHeight", "0");
shouldBe("testDiv.clientWidth", "0");
shouldBe("testDiv.clientHeight", "0");
shouldBe("testDiv.scrollLeft", "0");
shouldBe("testDiv.scrollTop", "0");
shouldBe("testDiv.scrollWidth", "0");
shouldBe("testDiv.scrollHeight", "0");

var successfullyParsed = true;
