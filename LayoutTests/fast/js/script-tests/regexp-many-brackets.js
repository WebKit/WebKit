description(
'Test regular expression processing with many capturing brackets (200).'
);

var count = 200;

var regexp = "";
for (var i = 0; i < count; ++i)
    regexp += "(";
regexp += "hello";
for (var i = 0; i < count; ++i)
    regexp += ")";

var manyHellosArray = new Array;
for (var i = 0; i <= count; ++i)
    manyHellosArray[i] = "hello";

var manyBracketsRegExp = new RegExp(regexp);
shouldBe("'hello'.match(manyBracketsRegExp)", "manyHellosArray");

debug('');

var successfullyParsed = true;
