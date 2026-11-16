function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldNotThrow(source) {
    try {
        (0, eval)(source);
    } catch (error) {
        throw new Error(`\`${source}\` threw ${String(error)}`);
    }
}

function shouldThrowSyntaxError(source) {
    let error;
    try {
        (0, eval)(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`\`${source}\` did not throw a SyntaxError: ${String(error)}`);
}

function shouldThrowModuleSyntaxError(source) {
    let error;
    try {
        checkModuleSyntax(source);
    } catch (e) {
        error = e;
    }
    if (!String(error).startsWith("SyntaxError"))
        throw new Error(`\`${source}\` did not throw a SyntaxError: ${String(error)}`);
}

const wrappers = [
    expression => `async function f(a, b) { ${expression}; }`,
    expression => `async function f(a, b) { return ${expression}; }`,
    expression => `(async function* (a, b) { yield ${expression}; });`,
    expression => `(async (a, b) => ${expression});`,
    expression => `(async (a, b) => { ${expression}; });`,
    expression => `({ async m(a, b) { ${expression}; } });`,
    expression => `(class { async m(a, b) { ${expression}; } });`,
    expression => `async function f(a, b) { for await (var x of ${expression}) ; }`,
];

const invalid = [
    "await a ** b",
    "await a\n** b",
    "await a ** await b",
    "await await a ** b",
    "await a++ ** b",
    "await a.b ** b",
    "await a() ** b",
    "await (a) ** b",
    "2 ** await a ** b",
    "x = await a ** b",
    "(await a ** b)",
    "[await a ** b]",
    "`${await a ** b}`",
];

const valid = [
    "(await a) ** b",
    "await (a ** b)",
    "a ** await b",
    "(await a) ** await b",
    "2 ** (await a) ** b",
    "await a * b",
];

for (let wrapper of wrappers) {
    for (let expression of invalid)
        shouldThrowSyntaxError(wrapper(expression));
    for (let expression of valid)
        shouldNotThrow(wrapper(expression));
}

for (let expression of invalid)
    shouldThrowModuleSyntaxError(`var a, b; ${expression};`);
for (let expression of valid)
    checkModuleSyntax(`var a, b; ${expression};`);

function identifier(await) {
    return await ** 2;
}
shouldBe(identifier(3), 9);

function field(await) {
    return (async () => new class { x = await ** 2; }().x)();
}

let result;
let error;
field(3).then(value => { result = value; }, e => { error = e; });
drainMicrotasks();
if (error)
    throw error;
shouldBe(result, 9);

async function values() {
    shouldBe((await 2) ** 3, 8);
    shouldBe(await (2 ** 3), 8);
    shouldBe(2 ** await 3, 8);
    shouldBe((await 2) ** await 3, 8);
}
values().then(undefined, e => { error = e; });
drainMicrotasks();
if (error)
    throw error;
