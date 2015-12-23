description("This test checks the behavior of String.prototype.localeCompare as described in the ECMAScript Internationalization API Specification (ECMA-402 2.0).");

shouldBe("String.prototype.localeCompare.length", "1");
shouldBeFalse("Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor(String.prototype, 'localeCompare').writable");

// Test RequireObjectCoercible.
shouldThrow("String.prototype.localeCompare.call()", "'TypeError: String.prototype.localeCompare requires that |this| not be undefined'");
shouldThrow("String.prototype.localeCompare.call(undefined)", "'TypeError: String.prototype.localeCompare requires that |this| not be undefined'");
shouldThrow("String.prototype.localeCompare.call(null)", "'TypeError: String.prototype.localeCompare requires that |this| not be null'");
shouldNotThrow("String.prototype.localeCompare.call({}, '')");
shouldNotThrow("String.prototype.localeCompare.call([], '')");
shouldNotThrow("String.prototype.localeCompare.call(NaN, '')");
shouldNotThrow("String.prototype.localeCompare.call(5, '')");
shouldNotThrow("String.prototype.localeCompare.call('', '')");
shouldNotThrow("String.prototype.localeCompare.call(() => {}, '')");

// Test toString fails.
shouldThrow("''.localeCompare.call({ toString() { throw 'thisFail' } }, '')", "'thisFail'");
shouldThrow("''.localeCompare({ toString() { throw 'thatFail' } })", "'thatFail'");
shouldNotThrow("''.localeCompare()");
shouldNotThrow("''.localeCompare(null)");

// Basic support.
shouldBeTrue('"a".localeCompare("aa") < 0');
shouldBeTrue('"a".localeCompare("b") < 0');

shouldBeTrue('"a".localeCompare("a") === 0');
shouldBeTrue('"a\u0308\u0323".localeCompare("a\u0323\u0308") === 0');

shouldBeTrue('"aa".localeCompare("a") > 0');
shouldBeTrue('"b".localeCompare("a") > 0');

// Uses Intl.Collator.
shouldThrow("'a'.localeCompare('b', '$')", "'RangeError: invalid language tag: $'");
shouldThrow("'a'.localeCompare('b', 'en', {usage: 'Sort'})", '\'RangeError: usage must be either "sort" or "search"\'');

shouldBe("'ä'.localeCompare('z', 'en')", "-1");
shouldBe("'ä'.localeCompare('z', 'sv')", "1");

shouldBe("'a'.localeCompare('b', 'en', { sensitivity:'base' })", "-1");
shouldBe("'a'.localeCompare('ä', 'en', { sensitivity:'base' })", "0");
shouldBe("'a'.localeCompare('A', 'en', { sensitivity:'base' })", "0");
shouldBe("'a'.localeCompare('ⓐ', 'en', { sensitivity:'base' })", "0");

shouldBe("'a'.localeCompare('b', 'en', { sensitivity:'accent' })", "-1");
shouldBe("'a'.localeCompare('ä', 'en', { sensitivity:'accent' })", "-1");
shouldBe("'a'.localeCompare('A', 'en', { sensitivity:'accent' })", "0");
shouldBe("'a'.localeCompare('ⓐ', 'en', { sensitivity:'accent' })", "0");

shouldBe("'a'.localeCompare('b', 'en', { sensitivity:'case' })", "-1");
shouldBe("'a'.localeCompare('ä', 'en', { sensitivity:'case' })", "0");
shouldBe("'a'.localeCompare('A', 'en', { sensitivity:'case' })", "-1");
shouldBe("'a'.localeCompare('ⓐ', 'en', { sensitivity:'case' })", "0");

shouldBe("'a'.localeCompare('b', 'en', { sensitivity:'variant' })", "-1");
shouldBe("'a'.localeCompare('ä', 'en', { sensitivity:'variant' })", "-1");
shouldBe("'a'.localeCompare('A', 'en', { sensitivity:'variant' })", "-1");
shouldBe("'a'.localeCompare('ⓐ', 'en', { sensitivity:'variant' })", "-1");

shouldBe("'1'.localeCompare('2', 'en', { numeric:false })", "-1");
shouldBe("'2'.localeCompare('10', 'en', { numeric:false })", "1");
shouldBe("'01'.localeCompare('1', 'en', { numeric:false })", "-1");
shouldBe("'๑'.localeCompare('๒', 'en', { numeric:false })", "-1");
shouldBe("'๒'.localeCompare('๑๐', 'en', { numeric:false })", "1");
shouldBe("'๐๑'.localeCompare('๑', 'en', { numeric:false })", "-1");

shouldBe("'1'.localeCompare('2', 'en', { numeric:true })", "-1");
shouldBe("'2'.localeCompare('10', 'en', { numeric:true })", "-1");
shouldBe("'01'.localeCompare('1', 'en', { numeric:true })", "0");
shouldBe("'๑'.localeCompare('๒', 'en', { numeric:true })", "-1");
shouldBe("'๒'.localeCompare('๑๐', 'en', { numeric:true })", "-1");
shouldBe("'๐๑'.localeCompare('๑', 'en', { numeric:true })", "0");

