description("Check the Date's prototype properties.");

// toGMTString.
shouldBe('Date.prototype.toGMTString', 'Date.prototype.toUTCString');

// toUTCString.
shouldBeEqualToString('Date.prototype.toUTCString.name', 'toUTCString');
shouldBe('Date.prototype.toUTCString.length', '0');
shouldBeEqualToString('typeof Date.prototype.toUTCString', 'function');
shouldBeTrue('Object.getOwnPropertyDescriptor(Date.prototype, "toUTCString").configurable');
shouldBeFalse('Object.getOwnPropertyDescriptor(Date.prototype, "toUTCString").enumerable');
shouldBeTrue('Object.getOwnPropertyDescriptor(Date.prototype, "toUTCString").writable');
