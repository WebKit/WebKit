description(
'This test checks that CSS property names work round trip in the transition property.'
);

var element = document.createElement('a');

element.style.webkitTransitionProperty = "height";
shouldBe("element.style.webkitTransitionProperty", "'height'");

element.style.webkitTransitionProperty = "opacity";
shouldBe("element.style.webkitTransitionProperty", "'opacity'");

var successfullyParsed = true;
