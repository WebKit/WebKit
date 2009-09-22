description(

"This test checks that toLowerCase and toUpperCase handle certain non-trivial cases correctly."

);

shouldBe('String("A𐐀").toLowerCase()', '"a𐐨"');
shouldBe('String("a𐐨").toUpperCase()', '"A𐐀"');
shouldBe('String("ΚΟΣΜΟΣ ΚΟΣΜΟΣ").toLowerCase()', '"κοσμος κοσμος"');
shouldBe('String("ß").toUpperCase()', '"SS"');
shouldBe('String("ŉ").toUpperCase()', '"ʼN"');
shouldBe('String("ǰ").toUpperCase()', '"J̌"');
shouldBe('String("ﬃ").toUpperCase()', '"FFI"');
shouldBe('String("FFI").toLowerCase()', '"ffi"');
shouldBe('String("Ĳ").toLowerCase()', '"ĳ"');

successfullyParsed = true;
