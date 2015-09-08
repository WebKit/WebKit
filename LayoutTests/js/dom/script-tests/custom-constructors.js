description(
"This test checks construction of objects with custom constructors."
);

// Image tests
shouldBeNonNull("new Image()");
shouldBeEqualToString("new Image().tagName", "IMG");

shouldBe("new Image().height", "0");
shouldBe("new Image().width", "0");
shouldBe("new Image(100).width", "100");
shouldBe("new Image(100, 200).height", "200");
shouldBe("new Image(-100).width", "-100");
shouldBe("new Image(-100, -200).height", "-200");

shouldBeEqualToString("new Image().outerHTML","<img>");
// FIXME: shouldBeEqualToString strips quotes from the string.
shouldBeEqualToString("new Image(100, 100).outerHTML.replace(/\"/g, \"'\")", "<img width='100' height='100'>");

// Option tests
shouldBeNonNull("new Option()");
shouldBeEqualToString("new Option().tagName", "OPTION");

shouldBeEqualToString("new Option().innerText", "");
shouldBeEqualToString("new Option(null).innerText", "null");
shouldBeEqualToString("new Option(undefined).innerText", "");
shouldBeEqualToString("new Option('somedata').innerText", "somedata");

shouldBeEqualToString("new Option().value", "");
shouldBeEqualToString("new Option('somedata', null).value", "null");
shouldBeEqualToString("new Option('somedata', undefined).value", "somedata");
shouldBeEqualToString("new Option('somedata', 'somevalue').value", "somevalue");

shouldBeFalse("new Option().defaultSelected");
shouldBeFalse("new Option('somedata', 'somevalue').defaultSelected");
shouldBeFalse("new Option('somedata', 'somevalue', false).defaultSelected");
shouldBeTrue("new Option('somedata', 'somevalue', true).defaultSelected");

shouldBeFalse("new Option().selected");
shouldBeFalse("new Option('somedata', 'somevalue').selected");
shouldBeFalse("new Option('somedata', 'somevalue', false).selected");
shouldBeFalse("new Option('somedata', 'somevalue', true).selected");
shouldBeFalse("new Option('somedata', 'somevalue', true, false).selected");
shouldBeTrue("new Option('somedata', 'somevalue', true, true).selected");

shouldBeEqualToString("new Option().outerHTML","<option></option>");
shouldBeEqualToString("new Option('somedata', 'somevalue', false).outerHTML.replace(/\"/g,\"'\")", "<option value='somevalue'>somedata</option>");
shouldBeEqualToString("new Option('somedata', 'somevalue', true).outerHTML.replace(/\"/g,\"'\")", "<option value='somevalue' selected=''>somedata</option>");

// Audio tests
shouldBeNonNull("new Audio()");
shouldBeEqualToString("new Audio().tagName", "AUDIO");

shouldBeEqualToString("new Audio().src", "");
shouldBeEqualToString("new Audio().preload", "auto");
shouldBeEqualToString("new Audio('http://127.0.0.1/someurl').src", "http://127.0.0.1/someurl");
shouldBeEqualToString("new Audio('http://127.0.0.1/someurl').preload", "auto");
