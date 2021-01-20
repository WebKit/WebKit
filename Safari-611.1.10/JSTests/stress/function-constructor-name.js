function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var GeneratorFunction = function*(){}.constructor;
var AsyncFunction = async function(){}.constructor;
var AsyncGeneratorFunction = async function*(){}.constructor;

var f = Function(`return 42`);
shouldBe(typeof anonymous, `undefined`);
shouldBe(f.toString(),
`function anonymous() {
return 42
}`);

var gf = GeneratorFunction(`return 42`);
shouldBe(typeof anonymous, `undefined`);
shouldBe(gf.toString(),
`function* anonymous() {
return 42
}`);

var af = AsyncFunction(`return 42`);
shouldBe(typeof anonymous, `undefined`);
shouldBe(af.toString(),
`async function anonymous() {
return 42
}`);

var agf = AsyncGeneratorFunction(`return 42`);
shouldBe(typeof anonymous, `undefined`);
shouldBe(agf.toString(),
`async function* anonymous() {
return 42
}`);
