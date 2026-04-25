description(
"Tests the behavior of Array.prototype.concat while the array is having a bad time due to one of the elements we are concatenating."
);

Object.defineProperty(Array.prototype, 0, { writable: false });
shouldBe("[42].concat()", "[42]");


