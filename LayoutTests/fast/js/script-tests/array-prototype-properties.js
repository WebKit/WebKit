description(
'This is a test case for <a https://bugs.webkit.org/show_bug.cgi?id=64679">bug 64679</a>.'
);

// These calls pass undefined as this value, and as such should show in toObject.
shouldThrow("Array.prototype.toString.call(undefined)");
shouldThrow("Array.prototype.toLocaleString.call(undefined)");
shouldThrow("Array.prototype.concat.call(undefined, [])");
shouldThrow("Array.prototype.join.call(undefined, [])");
shouldThrow("Array.prototype.pop.call(undefined)");
shouldThrow("Array.prototype.push.call(undefined, {})");
shouldThrow("Array.prototype.reverse.call(undefined)");
shouldThrow("Array.prototype.shift.call(undefined)");
shouldThrow("Array.prototype.slice.call(undefined, 0, 1)");
shouldThrow("Array.prototype.sort.call(undefined)");
shouldThrow("Array.prototype.splice.call(undefined, 0, 1)");
shouldThrow("Array.prototype.unshift.call(undefined, {})");
shouldThrow("Array.prototype.every.call(undefined, toString)");
shouldThrow("Array.prototype.forEach.call(undefined, toString)");
shouldThrow("Array.prototype.some.call(undefined, toString)");
shouldThrow("Array.prototype.indexOf.call(undefined, 0)");
shouldThrow("Array.prototype.indlastIndexOfexOf.call(undefined, 0)");
shouldThrow("Array.prototype.filter.call(undefined, toString)");
shouldThrow("Array.prototype.reduce.call(undefined, toString)");
shouldThrow("Array.prototype.reduceRight.call(undefined, toString)");
shouldThrow("Array.prototype.map.call(undefined, toString)");

var successfullyParsed = true;
