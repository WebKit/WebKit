description(
"This test checks the names of functions constructed two different ways."
);

document.documentElement.setAttribute("onclick", "2");

shouldBe("new Function('1').toString().replace(/[ \\n]+/g, ' ')", "'function anonymous() { 1; }'");
shouldBe("document.documentElement.onclick.toString().replace(/[ \\n]+/g, ' ')", "'function onclick(event) { 2; }'");

var successfullyParsed = true;
