description(
'This is a test of the charAt and charCodeAt string functions.'
);

var undefined;

var cases = [
    ["", "omitted"],
    ["", undefined],
    ["", 0],
    ["", null],
    ["", false],
    ["", true],
    ["", 0.0],
    ["", 0.1],
    ["", 999],
    ["", 1/0],
    ["", -1],
    ["", -1/0],
    ["", 0/0],

    ["x", "omitted"],
    ["x", undefined],
    ["x", 0],
    ["x", null],
    ["x", false],
    ["x", true],
    ["x", 0.0],
    ["x", 0.1],
    ["x", 999],
    ["x", 1/0],
    ["x", -1],
    ["x", -1/0],
    ["x", 0/0],

    ["xy", "omitted"],
    ["xy", undefined],
    ["xy", 0],
    ["xy", null],
    ["xy", false],
    ["xy", true],
    ["xy", 0.0],
    ["xy", 0.1],
    ["xy", 999],
    ["xy", 1/0],
    ["xy", -1],
    ["xy", -1/0],
    ["xy", 0/0],
];

var answers = [['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['""', 'NaN'],
['"x"', '120'],
['"x"', '120'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"x"', '120'],
['"y"', '121'],
['"x"', '120'],
['"x"', '120'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['""', 'NaN'],
['"x"', '120']];

for (var i = 0; i < cases.length; ++i)
{
    var item = cases[i];
    var result = answers[i];
    if (item[1] == "omitted") {
        shouldBe('"' + item[0] + '".charAt()', result[0]);
        if (result[1] == 'NaN')
            shouldBeNaN('"' + item[0] + '".charCodeAt()');
        else
            shouldBe('"' + item[0] + '".charCodeAt()', result[1]);
    } else {
        shouldBe('"' + item[0] + '".charAt(' + item[1] + ')', result[0]);
        if (result[1] == 'NaN')
            shouldBeNaN('"' + item[0] + '".charCodeAt(' + item[1] + ')');
        else
            shouldBe('"' + item[0] + '".charCodeAt(' + item[1] + ')', result[1]);
    }
}

var successfullyParsed = true;
