Array.prototype.valueOf = function() { return 99; };

const arr = [1, 2, 3];
const slowArr = [1, 2, 3];
slowArr.__proto__ = Object.create(Array.prototype);

if ("" + arr !== "99")
    throw `Expected "99" for arr, got "${"" + arr}"`;

if ("" + slowArr !== "99")
    throw `Expected "99" for slowArr, got "${"" + slowArr}"`;