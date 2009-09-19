description("Test for <a href='https://bugs.webkit.org/show_bug.cgi?id=21903'>https://bugs.webkit.org/show_bug.cgi?id=21903</a>");

var input = document.createElement("input");

shouldBeEqualToString("input.placeholder", '');

input.setAttribute("placeholder", "p1")
shouldBeEqualToString("input.placeholder", 'p1');

input.placeholder = 'p2';
shouldBeEqualToString("input.getAttribute('placeholder')", 'p2');
shouldBeEqualToString("input.placeholder", 'p2');

var successfullyParsed = true;
