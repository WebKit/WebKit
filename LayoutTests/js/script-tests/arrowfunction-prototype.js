// Inspired by mozilla tests
description('Tests for ES6 arrow function prototype property');

var af1 = () =>  {};

debug('() =>  {}');
shouldBe("typeof af1.prototype", "'undefined'");
shouldBe("af1.hasOwnProperty('prototype')", "false");

var af2 = (a) => {a + 1};

debug('(a) => {a + 1}');
shouldBe("typeof af2.prototype", "'undefined'");
shouldBe("af2.hasOwnProperty('prototype')", "false");

var af3 = (x) =>  x + 1;

debug('(x) =>  x + 1');
shouldBe("typeof af3.prototype", "'undefined'");
shouldBe("af3.hasOwnProperty('prototype')", "false");


af1.prototype = function (x) { return x + 1;};

debug('af1.prototype = function (x) { return x + 1;}');
shouldBe("typeof af1.prototype", "'function'");
shouldBe("af1.prototype.toString()", "'function (x) { return x + 1;}'");
shouldBe("af1.hasOwnProperty('prototype')", "true");

delete af1.prototype;

debug('delete af1.prototype');
shouldBe("typeof af1.prototype", "'undefined'");
shouldBe("af1.hasOwnProperty('prototype')", "false");

var successfullyParsed = true;
