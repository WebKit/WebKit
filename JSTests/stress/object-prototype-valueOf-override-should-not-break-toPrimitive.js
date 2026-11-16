Object.prototype.valueOf = function() { return 42; };

const arr = [1, 2, 3];
const slowArr = [1, 2, 3];
slowArr.__proto__ = Object.create(Array.prototype);

if ("" + arr !== "42")
    throw `Expected "42" for arr, got "${"" + arr}"`;

if ("" + slowArr !== "42")
    throw `Expected "42" for slowArr, got "${"" + slowArr}"`;
