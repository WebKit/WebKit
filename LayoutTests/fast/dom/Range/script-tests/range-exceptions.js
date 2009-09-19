description(
"This test checks some DOM Range exceptions."
);

// Test to be sure the name BAD_BOUNDARYPOINTS_ERR dumps properly.
var node = document.createElement("DIV");
node.innerHTML = "<BAR>AB<MOO>C</MOO>DE</BAR>";
shouldBe("node.innerHTML", "'<bar>AB<moo>C</moo>DE</bar>'");
var range = document.createRange();
range.setStart(node.firstChild, 1);
range.setEnd(node.firstChild, 2);
var foo = document.createElement("FOO");
shouldBe("foo.outerHTML", "'<foo></foo>'");
shouldThrow("range.surroundContents(foo)");

// Ensure that we throw BAD_BOUNDARYPOINTS_ERR when trying to split a comment
// (non-text but character-offset node). (Test adapted from Acid3.)
var c1 = document.createComment("aaaaa");
node.appendChild(c1);
var c2 = document.createComment("bbbbb");
node.appendChild(c2);
var r = document.createRange();
r.setStart(c1, 2);
r.setEnd(c2, 3);
shouldThrow("r.surroundContents(document.createElement('a'))", '"Error: BAD_BOUNDARYPOINTS_ERR: DOM Range Exception 1"');

// But not when we don't try to split the comment.
r.setStart(c1, 0);
r.setEnd(c1, 5);
shouldThrow("r.surroundContents(document.createElement('a'))", '"Error: HIERARCHY_REQUEST_ERR: DOM Exception 3"');

var successfullyParsed = true;
