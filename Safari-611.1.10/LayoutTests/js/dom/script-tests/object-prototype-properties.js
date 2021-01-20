description(
'This is a test case for <a https://bugs.webkit.org/show_bug.cgi?id=64678">bug 64678</a>.'
);

//ES 5.1 allows for Object.prototype.toString being called with undefined/null.
shouldBe("Object.prototype.toString.call(undefined)", '"[object Undefined]"');
shouldBe("Object.prototype.toString.call(null)", '"[object Null]"');

// These calls pass undefined as this value, and as such should throw in toObject.
shouldThrow("Object.prototype.toLocaleString.call(undefined)");
shouldThrow("Object.prototype.valueOf.call(undefined)");
shouldThrow("Object.prototype.hasOwnProperty.call(undefined, 'hasOwnProperty')");
shouldThrow("Object.prototype.propertyIsEnumerable.call(undefined, 'propertyIsEnumerable')");
shouldThrow("Object.prototype.isPrototypeOf.call(undefined, this)");
