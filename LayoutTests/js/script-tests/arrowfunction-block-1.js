description('Tests for ES6 arrow function declaration body as block');

debug('f = () => {}');
var f = () => {};
shouldBe("typeof f()", "'undefined'");

debug('g = () => ({})');
var g = () => ({});
shouldBe("typeof g()", "'object'");

var successfullyParsed = true;
