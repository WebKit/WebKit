description(
'Test for beginning of line (BOL or ^) matching</a>'
);

var s = "abc123def456xyzabc789abc999";
shouldBeNull('s.match(/^notHere/)');
shouldBe('s.match(/^abc/)', '["abc"]');
shouldBe('s.match(/(^|X)abc/)', '["abc",""]');
shouldBe('s.match(/^longer|123/)', '["123"]');
shouldBe('s.match(/(^abc|c)123/)', '["abc123","abc"]');
shouldBe('s.match(/(c|^abc)123/)', '["abc123","abc"]');
shouldBe('s.match(/(^ab|abc)123/)', '["abc123","abc"]');
shouldBe('s.match(/(bc|^abc)([0-9]*)a/)', '["bc789a","bc","789"]');
shouldBeNull('/(?:(Y)X)|(X)/.exec("abc")');
shouldBeNull('/(?:(?:^|Y)X)|(X)/.exec("abc")');
shouldBeNull('/(?:(?:^|Y)X)|(X)/.exec("abcd")');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("Xabcd")', '["X",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("aXbcd")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abXcd")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcXd")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcdX")', '["X","X"]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("YXabcd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("aYXbcd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abYXcd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcYXd")', '["YX",undefined]');
shouldBe('/(?:(?:^|Y)X)|(X)/.exec("abcdYX")', '["YX",undefined]');

var successfullyParsed = true;
