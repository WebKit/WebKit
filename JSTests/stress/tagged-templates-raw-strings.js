function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + JSON.stringify(actual));
}

function tag(results) {
    return function (siteObject) {
        shouldBe(siteObject.raw.length, results.length);
        for (var i = 0; i < siteObject.raw.length; ++i) {
            shouldBe(siteObject.raw[i], results[i]);
        }
    };
}

tag([''])``;
tag(['hello'])`hello`;
tag(['hello', 'world'])`hello${0}world`;
tag(['hello\\u2028', 'world'])`hello\u2028${0}world`;
tag(['hello\\u2028\\u2029', 'world'])`hello\u2028\u2029${0}world`;
tag(['hello\\n\\r', 'world'])`hello\n\r${0}world`;

function testEval(content, results) {
    var split = 0;
    var g = tag(results);
    eval("g`" + content + "`");
}

for (var ch of [ '\'', '"', '\\', 'b', 'f', 'n', 'r', 't', 'v' ])
    testEval("\\" + ch, ["\\" + ch]);

var evaluated = [];
for (var i = 0; i < 0x10000; ++i) {
    var code = i.toString(16);
    var input = "\\u" + '0'.repeat(4 - code.length) + code;
    evaluated.push(input);
}
testEval(evaluated.join('${split}'), evaluated)

testEval("Hello\rWorld", [ "Hello\nWorld" ]);
testEval("Hello\nWorld", [ "Hello\nWorld" ]);

testEval("Hello\r\rWorld", [ "Hello\n\nWorld" ]);
testEval("Hello\r\nWorld", [ "Hello\nWorld" ]);
testEval("Hello\n\nWorld", [ "Hello\n\nWorld" ]);
testEval("Hello\n\rWorld", [ "Hello\n\nWorld" ]);

testEval("Hello\n\r\nWorld", [ "Hello\n\nWorld" ]);
testEval("Hello\r\n\rWorld", [ "Hello\n\nWorld" ]);
testEval("Hello\n\n\nWorld", [ "Hello\n\n\nWorld" ]);

testEval("Hello\n\r\n\rWorld", [ "Hello\n\n\nWorld" ]);
testEval("Hello\n\r\n\nWorld", [ "Hello\n\n\nWorld" ]);
testEval("Hello\r\n\n\nWorld", [ "Hello\n\n\nWorld" ]);

testEval("Hello\\\n\r\rWorld", [ "Hello\\\n\n\nWorld" ]);
testEval("Hello\\\r\n\n\nWorld", [ "Hello\\\n\n\nWorld" ]);
testEval("Hello\\\n\r\n\nWorld", [ "Hello\\\n\n\nWorld" ]);
testEval("Hello\\\n\r\r\nWorld", [ "Hello\\\n\n\nWorld" ]);

testEval("\u2028", [ "\u2028" ]);
testEval("\u2029", [ "\u2029" ]);
testEval("\\u2028", [ "\\u2028" ]);
testEval("\\u2029", [ "\\u2029" ]);
