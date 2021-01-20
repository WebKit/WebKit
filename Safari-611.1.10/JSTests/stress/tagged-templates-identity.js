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
shouldBe(templates[0] !== templates[2], true);

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
shouldBe(templates[0] !== templates[1], true);
shouldBe(templates[0] !== templates[2], true);

templates = [];
eval("tag`Hello\n`");
eval("tag`Hello\\n`");
eval("tag`Hello\r`");
eval("tag`Hello\\r`");
shouldBe(templates.length, 4);
shouldBe(templates[0] !== templates[1], true);
shouldBe(templates[0] !== templates[2], true);
shouldBe(templates[0] !== templates[3], true);
shouldBe(templates[1] !== templates[2], true);
shouldBe(templates[1] !== templates[3], true);
shouldBe(templates[2] !== templates[3], true);

var v = 0;
templates = [];
eval("tag`Hello\n${v}world`");
eval("tag`Hello\n${v}world`");
shouldBe(templates.length, 2);
shouldBe(templates[0] !== templates[1], true);

var v = 0;
templates = [];
eval("tag`Hello${v}\nworld`");
eval("tag`Hello\n${v}world`");
shouldBe(templates.length, 2);
shouldBe(templates[0] !== templates[1], true);

var v = 0;
templates = [];
for (v = 0; v < 3; ++v) {
    tag`Hello${v}world`;
    if (!v) continue;
    shouldBe(templates[v] === templates[v - 1], true);
}

templates = [];
tag`Hello${1}world`;
tag`Hello${2}world`;
shouldBe(templates[0] !== templates[1], true);

// Disable eval caching if a tagged template occurs in eval code
var v = 0;
templates = [];
for (v = 0; v < 3; ++v) {
    eval("tag`Hello${v}world`");
    if (!v) continue;
    shouldBe(templates[v] !== templates[v - 1], true);
}

templates = [];
eval("tag`Hello${1}world`");
eval("tag`Hello${2}world`");
eval("tag`Hello${3}world`");
shouldBe(templates[0] !== templates[1], true);
shouldBe(templates[1] !== templates[2], true);


// Disable eval caching if a tagged template occurs in a nested function
var v = 0;
templates = [];
for (v = 0; v < 6; v += 2) {
    eval("(function() { for (var i = 0; i < 2; ++i) tag`Hello${v}world` })()");
    shouldBe(templates[v] === templates[v + 1], true);
}

shouldBe(templates[0] !== templates[2], true);
shouldBe(templates[0] !== templates[4], true);
shouldBe(templates[1] !== templates[3], true);
shouldBe(templates[1] !== templates[5], true);
shouldBe(templates[2] !== templates[4], true);
shouldBe(templates[3] !== templates[5], true);

templates = [];
eval("(function() { for (var i = 0; i < 2; ++i) tag`Hello${1}world` })()");
eval("(function() { for (var i = 0; i < 2; ++i) tag`Hello${2}world` })()");
eval("(function() { for (var i = 0; i < 2; ++i) tag`Hello${2}world` })()");
shouldBe(templates[0] === templates[1], true);
shouldBe(templates[0] !== templates[2], true);
shouldBe(templates[0] !== templates[4], true);
shouldBe(templates[1] !== templates[3], true);
shouldBe(templates[1] !== templates[5], true);
shouldBe(templates[2] === templates[3], true);
shouldBe(templates[2] !== templates[4], true);
shouldBe(templates[3] !== templates[5], true);
shouldBe(templates[4] === templates[5], true);

// Disable eval caching if a tagged template occurs in an even deeper nested function
var v = 0;
templates = [];
for (v = 0; v < 3; ++v) {
    eval("(function() { (function() { tag`Hello${v}world` })() })()");
    if (!v) continue;
    shouldBe(templates[v] !== templates[v - 1], true);
}

templates = [];
eval("(function() { (function() { for (var i = 0; i < 2; ++i) tag`Hello${1}world` })() })()");
eval("(function() { (function() { for (var i = 0; i < 2; ++i) tag`Hello${2}world` })() })()");
eval("(function() { (function() { for (var i = 0; i < 2; ++i) tag`Hello${2}world` })() })()");
shouldBe(templates[0] === templates[1], true);
shouldBe(templates[0] !== templates[2], true);
shouldBe(templates[0] !== templates[4], true);
shouldBe(templates[1] !== templates[3], true);
shouldBe(templates[1] !== templates[5], true);
shouldBe(templates[2] === templates[3], true);
shouldBe(templates[2] !== templates[4], true);
shouldBe(templates[3] !== templates[5], true);
shouldBe(templates[4] === templates[5], true);
