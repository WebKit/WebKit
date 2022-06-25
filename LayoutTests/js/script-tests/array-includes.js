description("Tests for Array.prototype.includes");

shouldBe("Array.prototype.includes.length", "1");
shouldBe("Array.prototype.includes.name", "'includes'");

shouldBeTrue("[1, 2, 3].includes(2)");
shouldBeFalse("[1, 2, 3].includes(4)");
shouldBeFalse("[].includes(1)");

shouldBeFalse("[1, 2, 3].includes(1, 2)");
shouldBeFalse("[1, 2, 3].includes(3, 3)");
shouldBeTrue("[1, 2, 3].includes(2, undefined)");
shouldBeTrue("[1, 2, 3].includes(2, null)");
shouldBeTrue("[1, 2, 3].includes(2, 1, 2)");
shouldBeTrue("[1, 2, 3].includes(2, Number)");
shouldBeFalse("[1, 2, 3].includes(2, Number(2))");
shouldBeTrue("[1, 2, 3].includes(2, 'egg')");
shouldBeFalse("[1, 2, 3].includes(2, '3')");

shouldBeTrue("[1, 2, 3].includes(3, -1)");
shouldBeFalse("[1, 2, 3].includes(1, -2)");
shouldBeTrue("[1, 2, 3].includes(1, -3)");

shouldBeTrue("[1, 2, NaN, 4].includes(NaN)");

shouldBeTrue("['egg', 'bacon', 'sausage'].includes('egg')");
shouldBeFalse("['egg', 'bacon', 'sausage'].includes('spinach')");

debug("Array with holes (sparse array)");

var a = [];
a[0] = 'egg';
a[1] = 'bacon';
a[5] = 'sausage';
a[6] = 'spinach';
a[-2] = 'toast';

shouldBeTrue("a.includes('egg')");
shouldBeTrue("a.includes('sausage')");
shouldBeFalse("a.includes('hashbrown')");
shouldBeFalse("a.includes('toast')");

shouldThrow("Array.prototype.includes.call(undefined, 1)");
shouldThrow("Array.prototype.includes.call(null, 1)");

debug("Array-like object with invalid lengths");
shouldBeFalse("var obj = { 0: 1, 1: 1, 2: 1, length: 0 }; Array.prototype.includes.call(obj, 1)");
shouldBeFalse("var obj = { 0: 1, 1: 1, 2: 1, length: -0 }; Array.prototype.includes.call(obj, 1)");
shouldBeFalse("var obj = { 0: 1, 1: 1, 2: 1, length: -3 }; Array.prototype.includes.call(obj, 1)");

debug("The index is converted to integer");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(2, NaN)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, NaN)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, NaN)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(2, Infinity)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(7, Infinity)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(17, Infinity)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(2, -Infinity)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, -Infinity)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, -Infinity)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(2, Number.MAX_SAFE_INTEGER)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(7, Number.MAX_SAFE_INTEGER)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(17, Number.MAX_SAFE_INTEGER)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(2, Number.MAX_SAFE_INTEGER + 1)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(7, Number.MAX_SAFE_INTEGER + 1)");
shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(17, Number.MAX_SAFE_INTEGER + 1)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(2, Number.MIN_SAFE_INTEGER)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, Number.MIN_SAFE_INTEGER)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, Number.MIN_SAFE_INTEGER)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(2, Number.MIN_SAFE_INTEGER - 1)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, Number.MIN_SAFE_INTEGER - 1)");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, Number.MIN_SAFE_INTEGER - 1)");

shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(2, { valueOf: () => { return 1; } })");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, { valueOf: () => { return 1; } })");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, { valueOf: () => { return 1; } })");

shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(2, { toString: () => { return '1'; } })");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, { toString: () => { return '1'; } })");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, { toString: () => { return '1'; } })");

shouldBeFalse("[2, 3, 5, 7, 11, 13, 17].includes(2, '1')");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(7, '1')");
shouldBeTrue("[2, 3, 5, 7, 11, 13, 17].includes(17, '1')");
