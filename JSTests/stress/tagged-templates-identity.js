function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + JSON.stringify(actual));
}

var templates = [];
function tag(siteObject) {
    templates.push(siteObject);
}

tag`Hello`;
tag`World`;
tag`Hello`;
shouldBe(templates.length, 3);
shouldBe(templates[0] !== templates[1], true);
shouldBe(templates[0] === templates[2], true);

templates = [];
tag`Hello\n`;
tag`Hello\r`;
tag`Hello\u2028`;
shouldBe(templates.length, 3);
shouldBe(templates[0] !== templates[1], true);
shouldBe(templates[0] !== templates[2], true);

templates = [];
eval("tag`Hello\n`");
eval("tag`Hello\r`");
eval("tag`Hello\u2028`");
shouldBe(templates.length, 3);
shouldBe(templates[0] === templates[1], true);
shouldBe(templates[0] !== templates[2], true);

templates = [];
eval("tag`Hello\n`");
eval("tag`Hello\\n`");
eval("tag`Hello\r`");
eval("tag`Hello\\r`");
shouldBe(templates.length, 4);
shouldBe(templates[0] !== templates[1], true);
shouldBe(templates[0] === templates[2], true);
shouldBe(templates[0] !== templates[3], true);
shouldBe(templates[1] !== templates[2], true);
shouldBe(templates[1] !== templates[3], true);
shouldBe(templates[2] !== templates[3], true);

var v = 0;
templates = [];
eval("tag`Hello\n${v}world`");
eval("tag`Hello\n${v}world`");
shouldBe(templates.length, 2);
shouldBe(templates[0] === templates[1], true);

var v = 0;
templates = [];
eval("tag`Hello${v}\nworld`");
eval("tag`Hello\n${v}world`");
shouldBe(templates.length, 2);
shouldBe(templates[0] !== templates[1], true);
