function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

function check(text) {
    shouldBe(JSON.stringify(JSON.parse(text)), JSON.stringify(JSON.parse(text, (key, value) => value)));
}

function checkThrows(text) {
    let threw = false;
    try {
        JSON.parse(text);
    } catch (error) {
        threw = error instanceof SyntaxError;
    }
    if (!threw)
        throw new Error(`should throw SyntaxError: ${text}`);
}

const wide = "あ";
const names = [
    "a", "id", "name", "abcdefg", "abcdefgh", "abcdefghi", "abcdefghijklmno", "abcdefghijklmnop",
    "abcdefghijklmnopq", "abcdefghijklmnopqrstuvwxyz0123456789",
];

for (let i = 0; i < testLoopCount; ++i) {
    for (const name of names) {
        const repeated = `[${Array.from({ length: 4 }, (_, j) => `{"${name}":"${wide}${j}","next":${j}}`).join(",")}]`;
        check(repeated);

        check(`[{"${name}":1,"x":"${wide}"},{"${name}x":3,"x":4},{"${name.slice(0, -1)}":5,"x":6}]`);
        check(`[{"${name}":1},{"${name}":"${wide}"},{"${name.slice(0, -1)}\\"":2},{"${name}\\\\":3},{"${name}\\u0041":4}]`);
        check(`[{"${name}":"${wide}"},{"${name.slice(0, -1)}\\u${name.charCodeAt(name.length - 1).toString(16).padStart(4, "0")}":2}]`);
        check(`[{"${name}":1},{"${name}${wide}":2},{"${wide}${name}":3}]`);
        check(`[{"${name}":"${wide}"},{"${name}":1}]`);
        check(`[{"${name}":"${wide}"},{"${name}"  :  1}]`);
        check(`[{"${name}":"${wide}"},{ "${name}":1}]`);

        const tail = `[{"${name}":"${wide}"},{"${name}":1}]`;
        for (let cut = 1; cut < tail.length; ++cut)
            checkThrows(tail.slice(0, cut));
    }

    check(`[{"${wide}":1,"a":2},{"${wide}":3,"a":4},{"${wide}${wide}":5}]`);
    check(`[{"été":1,"${wide}":2},{"été":3,"${wide}":4}]`);
    check(`[{"a":{"b":{"c":"${wide}"}}},{"a":{"b":{"c":1}}},{"a":{"b":{"d":1}}}]`);
    check(`[{"a":1,"a":2},{"a":3,"a":"${wide}"}]`);
    check(`[{"0":1,"1":2},{"0":3,"1":"${wide}"}]`);
    check(`[{"__proto__":1},{"__proto__":"${wide}"}]`);
    check(`[{"abcdefghijklmnopqrstuvwxyz":"${wide}"},{"abcdefghijklmnopqrstuvwxyz":1}]`.padEnd(200, " "));
    check(`[{"a":1},{"":5,"x":6},{"":5,"x":6},{"b":1},{"":2}]`);
    check(`[{"a":1},{"":5,"x":6},{"":5,"x":6},{"b":1},{"":"${wide}"}]`);
    checkThrows(`[{"a":1},{"a\n":"${wide}"}]`);
    checkThrows(`[{"abcdefghijk":1},{"abcdefghijk\u0001":"${wide}"}]`);
    checkThrows(`[{"a":1},{"a":"${wide}"},{"a"}]`);
}
