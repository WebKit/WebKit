
function test(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

function testEval(script, expected) {
    test(eval(script), expected);
}

function testEmbedded(value, expected) {
    var template = `Hello ${value} World`;
    test(template, expected);
}

test(``, "");
test(`${""}`, "");
test(`${``}`, "");
test(`${``}${``}${``}${""}`, "");

test(`Hello World`, "Hello World");
test(`Hello
        World`, "Hello\n        World");

test(`\uFEFF`, "\uFEFF");
testEval("`\uFEFF`", "\uFEFF");

test(`\x20`, "\x20");
test(`\x2020`, "\x2020");

for (var ch of [ '\'', '"', '\\', 'b', 'f', 'n', 'r', 't', 'v' ])
    testEval("`\\" + ch + "`", eval("'\\" + ch + "'"));

test(`\
Hello World`, "Hello World");

test(`\

Hello World`, "\nHello World");

test(`\u2028\u2029\r\n`, "\u2028\u2029\r\n");
test(`\u2028\u2029\n\r\n`, "\u2028\u2029\n\r\n");
test(`\u2028200`, "\u2028200");

testEmbedded(42, "Hello 42 World");
testEmbedded("ISUCA", "Hello ISUCA World");
testEmbedded(null, "Hello null World");
testEmbedded(undefined, "Hello undefined World");
testEmbedded({}, "Hello [object Object] World");

(function () {
    var object = {
        name: "Cocoa",
    };
    var array = [
        "Cappuccino"
    ]
    test(`Hello ${object.name} and ${array[0]} :D`, "Hello Cocoa and Cappuccino :D");
}());

(function () {
    function ok() {
        return "Cocoa";
    }
    test(`Hello ${ ok() }`, "Hello Cocoa");
}());

(function () {
    var object = {
        toString() {
            return 'Cocoa';
        }
    };
    test(`Hello ${object} :D`, "Hello Cocoa :D");
}());

// Immediately adjacent template expressions, with empty intermediate template strings.
test(`${"C"}${"o"}${"c"}${"o"}${"a"}`, "Cocoa");
test(`${"C"}${"o"}${"c"}${"o"}${" "}`, "Coco ");

// Nested templates.
test(`Hello ${ `Cocoa` }`, "Hello Cocoa");
test(`Hello ${ `Co${`c`}oa` }`, "Hello Cocoa");

// Evaluation order
(function () {
    var count = 0;

    function check(num) {
        var now = count++;
        test(now, num);
        return now;
    }

    test(`Hello ${ `${ check(0) } ${ ` ${ check(1) } ${ check(2) }` } ${ check(3) }` } ${ check(4) }`, "Hello 0  1 2 3 4")
}());

// Exceptions

(function () {
    function gen(num) {
        return {
            toString() {
                throw new Error(num);
            }
        };
    }

    var error = null;
    try {
        var template = `${gen(0)} ${gen(1)} ${gen(2)}`;
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('no error thrown');
    if (String(error) !== 'Error: 0')
        throw new Error("bad error: " + String(error));

    var error = null;
    try {
        var template = `${52} ${gen(0)} ${gen(1)}`;
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('no error thrown');
    if (String(error) !== 'Error: 0')
        throw new Error("bad error: " + String(error));
}());


(function () {
    var stat= {
        count: 0
    };
    function gen(num) {
        stat[num] = {
            called: true,
            count: stat.count++,
            toString: null
        };
        return {
            toString() {
                stat[num].toString = {
                    called: true,
                    count: stat.count++,
                };
                throw new Error(num);
            }
        };
    }

    var error = null;
    try {
        var template = `${gen(0)} ${gen(1)} ${gen(2)}`;
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('no error thrown');
    if (String(error) !== 'Error: 0')
        throw new Error("bad error: " + String(error));
    test(stat[0].count, 0);
    test(stat[0].toString.called, true);
    test(stat[0].toString.count, 1);
    test(stat[1], undefined);
    test(stat[2], undefined);
}());

(function () {
    var stat= {
        count: 0
    };
    function gen(num) {
        stat[num] = {
            called: true,
            count: stat.count++,
            toString: null
        };
        return {
            toString() {
                stat[num].toString = {
                    called: true,
                    count: stat.count++,
                };
                throw new Error(num);
            }
        };
    }

    var error = null;
    try {
        var template = `${ `${gen(0)}` } ${ `${gen(1)} ${gen(2)}` }`;
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error('no error thrown');
    if (String(error) !== 'Error: 0')
        throw new Error("bad error: " + String(error));
    test(stat[0].count, 0);
    test(stat[0].toString.called, true);
    test(stat[0].toString.count, 1);
    test(stat[1], undefined);
    test(stat[2], undefined);
}());

dfgTests =[
    function testSingleNode() {
        for (var i = 0; i < 1000; i++)
            `${1}`
    },
    function testPreNode() {
        for (var i = 0; i < 1000; i++)
            `n${1}`
    },
    function testPostNode() {
        for (var i = 0; i < 1000; i++)
            `${1}n`
    },
    function testSingleObjectNode() {
        for (var i = 0; i < 1000; i++)
            `${{}}`
    },
    function testObjectPreNode() {
        for (var i = 0; i < 1000; i++)
            `n${{}}`
    },
    function testObjectPostNode() {
        for (var i = 0; i < 1000; i++)
            `${{}}n`
    },
];

for(var f of dfgTests) {
    noInline(f)
    f();
}