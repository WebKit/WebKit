description("Test for bug 31689: RegExp#exec's returned Array-like object behaves differently from regular Arrays");

var tests = [
    [ /(a)(_)?.+(c)(_)?.+(e)(_)?.+/, 'abcdef', '["abcdef", "a", undefined, "c", undefined, "e", undefined]' ],
    [ /(a)(_)?/, 'abcdef', '["a", "a", undefined]' ],
    [ /(_)?.+(a)/, 'xabcdef', '["xa", undefined, "a"]' ],
    [ /(_)?.+(a)(_)?/, 'xabcdef', '["xa", undefined, "a", undefined]' ],
];

function testRegExpMatchesArray(i)
{
    return tests[i][0].exec(tests[i][1]);
}

function testInOperator(i)
{
    var re = tests[i][0],
        str = tests[i][1],
        inArray = [],
        matches = re.exec(str);

    for (var j = 0; j < matches.length; j++) {
        if (j in matches) {
            inArray.push(matches[j]);
        }
    }
    return inArray;
}

function testForEachFunction(i) 
{
    var re = tests[i][0],
        str = tests[i][1],
        inArray = [],
        matches = re.exec(str);

    matches.forEach(function(m) {
        inArray.push(m);
    });
    return inArray;

}

for (var i in tests) {
    shouldBe('testRegExpMatchesArray(' + i + ')', tests[i][2]);
    shouldBe('testInOperator(' + i + ')', tests[i][2]);
    shouldBe('testForEachFunction(' + i + ')', tests[i][2]);
}

