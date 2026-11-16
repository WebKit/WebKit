// Inspired by mozilla tests
description('Tests for ES6 arrow function instanceof and typeof operators');

var simpleArrowFunction = () => {};

shouldBe("Object.getPrototypeOf(simpleArrowFunction)", "Function.prototype");
shouldBe("simpleArrowFunction instanceof Function", "true");
shouldBe("simpleArrowFunction.constructor == Function", "true");

var successfullyParsed = true;
