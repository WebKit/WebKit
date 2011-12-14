description("This test checks that the type, media and title properties on an SVGStyleElement reflect the corresponding attributes.");

var style = document.createElementNS("http://www.w3.org/2000/svg", "style");

shouldBeEqualToString("style.type", "text/css");
shouldBeEqualToString("style.media", "all");
shouldBeEqualToString("style.title", "");

style.type = "text/x-something-else";
shouldBeEqualToString("style.type", "text/x-something-else");

style.media = "print";
shouldBeEqualToString("style.media", "print");

style.title = "My Style Sheet";
shouldBeEqualToString("style.title", "My Style Sheet");

successfullyParsed = true;
