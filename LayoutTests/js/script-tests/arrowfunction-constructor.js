description('Tests for ES6 arrow function no constructor');

var simpleArrowFunction = () => {};

shouldThrow('new simpleArrowFunction()');

var successfullyParsed = true;
