description(
"This test checks that a string conversion of an object that reaches itself throws a RangeError rather than silently substituting the empty string."
);

// Array (elements)
shouldThrow("var array = []; array[0] = array; array + ''");

// Error (name, message)
shouldThrow("var error = new Error; error.name = error; error.message = error; error + ''");

// RegExp (source). "source" is an inherited accessor with no setter, so this assignment is
// discarded in sloppy mode and no cycle is created.
shouldBe("var regexp = /a/; regexp.source = regexp; regexp + ''", "'/a/'");

// RegExp (source), this time actually cyclic.
shouldThrow("var regexp = /a/; Object.defineProperty(regexp, 'source', { get: function() { return regexp; } }); regexp + ''");

// Reaching an object whose conversion is already in progress is not by itself a cycle. Each of
// these terminates, so each must produce its ordinary result.
shouldBeEqualToString("var array = []; array[0] = { toString: function() { return Error.prototype.toString.call(array); } }; array.join()", "Error");
shouldBeEqualToString("var array = []; array[0] = { toString: function() { return RegExp.prototype.toString.call(array); } }; array.join()", "/undefined/undefined");
shouldBeEqualToString("var object = { length: 2, 0: 'x', 1: 'y' }; object.name = { toString: function() { return Array.prototype.join.call(object, '-'); } }; Error.prototype.toString.call(object)", "x-y");
