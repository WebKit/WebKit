description(
'This is a test case for <a https://bugs.webkit.org/show_bug.cgi?id=64677">bug 64677</a>.'
);

// These calls pass undefined as this value, and as such should show in toObject.
shouldThrow("String.prototype.toString.call(undefined)");
shouldThrow("String.prototype.valueOf.call(undefined)");
shouldThrow("String.prototype.charAt.call(undefined, 0)");
shouldThrow("String.prototype.charCodeAt.call(undefined, 0)");
shouldThrow("String.prototype.concat.call(undefined, 'five')");
shouldThrow("String.prototype.indexOf.call(undefined, '2')");
shouldThrow("String.prototype.lastIndexOf.call(undefined, '2')");
shouldThrow("String.prototype.match.call(undefined, /2+/)");
shouldThrow("String.prototype.replace.call(undefined, /2+/, '-')");
shouldThrow("String.prototype.search.call(undefined, '4')");
shouldThrow("String.prototype.slice.call(undefined, 1, 3)");
shouldThrow("String.prototype.split.call(undefined, '2')");
shouldThrow("String.prototype.slice.call(undefined, 1, 3)");
shouldThrow("String.prototype.substr.call(undefined, 1, 3)");
shouldThrow("String.prototype.substring.call(undefined, 1, 3)");
shouldThrow("String.prototype.toLowerCase.call(undefined)");
shouldThrow("String.prototype.toUpperCase.call(undefined)");
shouldThrow("String.prototype.localeCompare.call(undefined, '1224')");
shouldThrow("String.prototype.toLocaleLowerCase.call(undefined)");
shouldThrow("String.prototype.toLocaleUpperCase.call(undefined)");
shouldThrow("String.prototype.trim.call(undefined)");

// These calls pass a primitive number as this value, toString and valueOf
// should throw, all other method should convert ToString, without converting
// via the Number object type.
shouldThrow("String.prototype.toString.call(1224)");
shouldThrow("String.prototype.valueOf.call(1224)");
var numberPrototypeToString = Number.prototype.toString;
Number.prototype.toString = (function(){ throw "SHOULD NOT BE REACHED"; });
shouldBe("String.prototype.charAt.call(1224, 0)", '"1"');
shouldBe("String.prototype.charCodeAt.call(1224, 0)", '0x31');
shouldBe("String.prototype.concat.call(1224, 'five')", '"1224five"');
shouldBe("String.prototype.indexOf.call(1224, '2')", '1');
shouldBe("String.prototype.lastIndexOf.call(1224, '2')", '2');
shouldBe("String.prototype.match.call(1224, /2+/)", '["22"]');
shouldBe("String.prototype.replace.call(1224, /2+/, '-')", '"1-4"');
shouldBe("String.prototype.search.call(1224, '4')", '3');
shouldBe("String.prototype.slice.call(1224, 1, 3)", '"22"');
shouldBe("String.prototype.split.call(1224, '2')", '["1","","4"]');
shouldBe("String.prototype.slice.call(1224, 1, 3)", '"22"');
shouldBe("String.prototype.substr.call(1224, 1, 3)", '"224"');
shouldBe("String.prototype.substring.call(1224, 1, 3)", '"22"');
shouldBe("String.prototype.toLowerCase.call(1224)", '"1224"');
shouldBe("String.prototype.toUpperCase.call(1224)", '"1224"');
shouldBe("String.prototype.localeCompare.call(1224, '1224')", '0');
shouldBe("String.prototype.toLocaleLowerCase.call(1224)", '"1224"');
shouldBe("String.prototype.toLocaleUpperCase.call(1224)", '"1224"');
shouldBe("String.prototype.trim.call(1224)", '"1224"');
Number.prototype.toString = numberPrototypeToString;
