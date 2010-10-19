description("This tests support for the document.createTouch API.");

shouldBeTrue('"createTouch" in document');

var box = document.createElement("div");
box.id = "box";
box.style.width = "100px";
box.style.height = "100px";
document.body.appendChild(box);

var target = document.getElementById("box");
var touch = document.createTouch(window, target, 1, 100, 101, 102, 103);
shouldBeNonNull("touch");
shouldBe("touch.target", "box");
shouldBe("touch.identifier", "1");
shouldBe("touch.pageX", "100");
shouldBe("touch.pageY", "101");
shouldBe("touch.screenX", "102");
shouldBe("touch.screenY", "103");

var emptyTouch = document.createTouch();
shouldBeNonNull("emptyTouch");
shouldBeNull("emptyTouch.target");
shouldBe("emptyTouch.identifier", "0");
shouldBe("emptyTouch.pageX", "0");
shouldBe("emptyTouch.pageY", "0");
shouldBe("emptyTouch.screenX", "0");
shouldBe("emptyTouch.screenY", "0");

// Try invoking with incorrect parameter types.
var badParamsTouch = document.createTouch(function(x) { return x; }, 12, 'a', 'b', 'c', function(x) { return x; }, 104);
shouldBeNonNull("badParamsTouch");
shouldBeNull("badParamsTouch.target");
shouldBe("badParamsTouch.identifier", "0");
shouldBe("badParamsTouch.pageX", "0");
shouldBe("badParamsTouch.pageY", "0");
shouldBe("badParamsTouch.screenX", "0");
shouldBe("badParamsTouch.screenY", "104");

successfullyParsed = true;
isSuccessfullyParsed();
