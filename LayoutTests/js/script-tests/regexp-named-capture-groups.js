description(
'Test for of RegExp named capture groups'
);

// Verfiy that we can create group names and that we properly create the "groups" object,
// populated with the groups.
var re1 = new RegExp("(?<month>\\d{2})/(?<day>\\d{2})/(?<year>\\d{4})", "");
var src1 = "01/02/2001";
var execResult1 = re1.exec(src1);
shouldBe('re1.toString()', '"\\/(?<month>\\\\d{2})\\\\/(?<day>\\\\d{2})\\\\/(?<year>\\\\d{4})\\/"');
shouldBe('execResult1[0]', '"01/02/2001"');
shouldBe('execResult1.groups.month', '"01"');
shouldBe('execResult1.groups.day', '"02"');
shouldBe('execResult1.groups.year', '"2001"');
shouldBe('Object.getOwnPropertyNames(execResult1).sort()', '["0","1","2","3","groups","index","input","length"]');
shouldBe('Object.getOwnPropertyNames(execResult1.groups).sort()', '["day","month","year"]');

var matchResult1 = src1.match(re1);
shouldBe('matchResult1[0]', '"01/02/2001"');
shouldBe('matchResult1.groups.month', '"01"');
shouldBe('matchResult1.groups.day', '"02"');
shouldBe('matchResult1.groups.year', '"2001"');
shouldBe('Object.getOwnPropertyNames(matchResult1).sort()', '["0","1","2","3","groups","index","input","length"]');
shouldBe('Object.getOwnPropertyNames(matchResult1.groups).sort()', '["day","month","year"]');

var re2 = /(?<first_name>\w+)\s(?:(?<middle_initial>\w\.)\s)?(?<last_name>\w+)/;
var matchResult2a = "John W. Smith".match(re2);

shouldBe('matchResult2a[0]', '"John W. Smith"');
shouldBe('matchResult2a[1]', '"John"');
shouldBe('matchResult2a[2]', '"W."');
shouldBe('matchResult2a[3]', '"Smith"');
shouldBe('matchResult2a[1]', 'matchResult2a.groups.first_name');
shouldBe('matchResult2a[2]', 'matchResult2a.groups.middle_initial');
shouldBe('matchResult2a[3]', 'matchResult2a.groups.last_name');
shouldBe('Object.getOwnPropertyNames(matchResult1).sort()', '["0","1","2","3","groups","index","input","length"]');

// Verify that named groups that aren't matched are undefined.
var matchResult2b = "Sally Brown".match(re2);

shouldBe('matchResult2b[0]', '"Sally Brown"');
shouldBe('matchResult2b[1]', '"Sally"');
shouldBeUndefined('matchResult2b[2]');
shouldBe('matchResult2b[3]', '"Brown"');
shouldBe('matchResult2b[1]', 'matchResult2b.groups.first_name');
shouldBe('matchResult2b[2]', 'matchResult2b.groups.middle_initial');
shouldBe('matchResult2b[3]', 'matchResult2b.groups.last_name');
shouldBe('Object.getOwnPropertyNames(matchResult1).sort()', '["0","1","2","3","groups","index","input","length"]');

// Verify that named backreferences work.
var re3 = /^(?<part1>.*):(?<part2>.*):\k<part2>:\k<part1>$/;
shouldBe('re3.toString()', '"\\/^(?<part1>.*):(?<part2>.*):\\\\k<part2>:\\\\k<part1>$\\/"');
shouldBeTrue('re3.test("a:b:b:a")');
shouldBeTrue('re3.test("a:a:a:a")');
shouldBeFalse('re3.test("a:b:c:a")');

// Destructuring should work nicely with named groups.
var {groups: {first, second}} = /^(?<first>.*),(?<second>.*)$/u.exec('1,2');
shouldBe('first', '"1"');
shouldBe('second', '"2"');

// Check that unicode group names work.
let re4 = /(?<\u043c\u0435\u0441\u044f\u0446>\d{2})\/(?<\u0434\u0435\u043d\u044c>\d{2})\/(?<\u0433\u043e\u0434>\d{4})/;
var result4 = '02/14/2010'.replace(re4, (...args) => {
    let {\u0434\u0435\u043d\u044c, \u043c\u0435\u0441\u044f\u0446, \u0433\u043e\u0434} = args[args.length - 1];
    return `${\u0434\u0435\u043d\u044c}.${\u043c\u0435\u0441\u044f\u0446}.${\u0433\u043e\u0434}`;
});
shouldBe('result4', '"14.02.2010"');

// Verify that zero-width joiner and non-joiners can be used as part of a group name identifier
shouldBe('"third edition".match(/(?<auf\\u200clage>\\w+) edition/).groups.auf\\u200clage', '"third"');
shouldBe('"fourth edition".match(/(?<auf\\u200dlage>\\w+) edition/).groups.auf\\u200dlage', '"fourth"');

// Verify that both named and numeric group references work in a replacement string.
shouldBe('"10/20/1930".replace(/(?<month>\\d{2})\\\/(?<day>\\d{2})\\\/(?<year>\\d{4})/, "$<day>-$<month>-$<year>")', '"20-10-1930"');
shouldBe('"10/20/1930".replace(/(?<month>\\d{2})\\\/(?<day>\\d{2})\\\/(?<year>\\d{4})/, "$2-$<month>-$<year>")', '"20-10-1930"');
shouldBe('"10/20/1930".replace(/(?<month>\\d{2})\\\/(?<day>\\d{2})\\\/(?<year>\\d{4})/, "$<day>-$1-$<year>")', '"20-10-1930"');
shouldBe('"10/20/1930".replace(/(?<month>\\d{2})\\\/(?<day>\\d{2})\\\/(?<year>\\d{4})/, "$<day>-$<month>-$3")', '"20-10-1930"');

// Verify String.replace works correctly without named captures in the RegExp
shouldBe('"Replace just THIS in this string".replace(/THIS/, "$<THAT>")', '"Replace just $<THAT> in this string"');

// Verify that named back references for non-existing named group matches the k<groupName> for non-unicode patterns.
shouldBe('"Give me a \\\'k\\\'!".match(/Give me a \\\'\\\k\\\'/)[0]', '"Give me a \\\'k\\\'"');
shouldBe('"Give me \\\'k2\\\'!".match(/Give me \\\'\\\k2\\\'/)[0]', '"Give me \\\'k2\\\'"');
shouldBe('"Give me a \\\'kat\\\'!".match(/Give me a \\\'\\\kat\\\'/)[0]', '"Give me a \\\'kat\\\'"');
// Verify that named back references for non-existing named group matches the k<groupName> throw for unicode patterns.
shouldThrow('"Give me a \\\'k\\\'!".match(/Give me a \\\'\\\k\\\'/u)[0]', '"SyntaxError: Invalid regular expression: invalid escaped character for unicode pattern"');
shouldThrow('"Give me \\\'k2\\\'!".match(/Give me \\\'\\\k2\\\'/u)[0]', '"SyntaxError: Invalid regular expression: invalid escaped character for unicode pattern"');
shouldThrow('"Give me a \\\'kat\\\'!".match(/Give me a \\\'\\\kat\\\'/u)[0]', '"SyntaxError: Invalid regular expression: invalid escaped character for unicode pattern"');

// Check invalid group name specifiers in a replace string.
shouldBe('"10/20/1930".replace(/(?<month>\\d{2})\\\/(?<day>\\d{2})\\\/(?<year>\\d{4})/, "$<day>-$<mouth>-$<year>")', '"20--1930"');
shouldBe('"10/20/1930".replace(/(?<month>\\d{2})\\\/(?<day>\\d{2})\\\/(?<year>\\d{4})/, "$<day>-$<month>-$<year")', '"20-10-$<year"');

// Check invalid group name exceptions.
shouldThrow('let r = new RegExp("/(?<groupName1>abc)|(?<groupName1>def)/")', '"SyntaxError: Invalid regular expression: duplicate group specifier name"');
shouldThrow('let r = new RegExp("/(?< groupName1>abc)/")', '"SyntaxError: Invalid regular expression: invalid group specifier name"');
shouldThrow('let r = new RegExp("/(?<g=oupName1>abc)/")', '"SyntaxError: Invalid regular expression: invalid group specifier name"');

// And bad Unicode ID start and ID part
shouldThrow('let r = new RegExp("/(?<\u{10190}groupName1>abc)/u")', '"SyntaxError: Invalid regular expression: invalid group specifier name"');
shouldThrow('let r = new RegExp("/(?<g\u{1019b}oupName1>abc)/u")', '"SyntaxError: Invalid regular expression: invalid group specifier name"');
shouldThrow('let r = new RegExp("/(?<\u200cgroupName1>abc)/u")', '"SyntaxError: Invalid regular expression: invalid group specifier name"');
shouldThrow('let r = new RegExp("/(?<\u200dgroupName1>abc)/u")', '"SyntaxError: Invalid regular expression: invalid group specifier name"');

// Check the named forward references work
shouldBe('"XzzXzz".match(/\\\k<z>X(?<z>z*)X\\\k<z>/)', '["XzzXzz", "zz"]');
shouldBe('"XzzXzz".match(/\\\k<z>X(?<z>z*)X\\\k<z>/u)', '["XzzXzz", "zz"]');
shouldBe('"1122332211".match(/\\\k<ones>\\\k<twos>\\\k<threes>(?<ones>1*)(?<twos>2*)(?<threes>3*)\\\k<threes>\\\k<twos>\\\k<ones>/)', '["1122332211", "11", "22", "3"]');
shouldBe('"1122332211".match(/\\\k<ones>\\\k<twos>\\\k<threes>(?<ones>1*)(?<twos>2*)(?<threes>3*)\\\k<threes>\\\k<twos>\\\k<ones>/u)', '["1122332211", "11", "22", "3"]');

// Check that a named forward reference for a non-existent named capture
// matches for non-unicode patterns and throws for unicode patterns.
shouldBe('"\\\k<z>XzzX".match(/\\\k<z>X(z*)X/)', '["k<z>XzzX", "zz"]');
shouldThrow('"\\\k<z>XzzX".match(/\\\k<z>X(z*)X/u)', '"SyntaxError: Invalid regular expression: invalid backreference for unicode pattern"');
