description(
"This test checks a few DOM range exceptions."
);

// Test to be sure the name BAD_BOUNDARYPOINTS_ERR dumps properly.
var node = document.createElement("DIV");
node.innerHTML = "<BAR>AB<MOO>C</MOO>DE</BAR>";
shouldBe("node.innerHTML", "'<BAR>AB<MOO>C</MOO>DE</BAR>'");
var range = document.createRange();
range.setStart(node.firstChild, 1);
range.setEnd(node.firstChild, 2);
var foo = document.createElement("FOO");
shouldBe("foo.outerHTML", "'<FOO></FOO>'");
shouldThrow("range.surroundContents(foo)");

var successfullyParsed = true;
