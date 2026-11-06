//@ runDefault("--airRandomizeRegs=1", "--airRandomizeRegsSeed=3")
//@ runDefault("--airRandomizeRegs=1", "--airRandomizeRegsSeed=8")
//@ runDefault("--airRandomizeRegs=1", "--airRandomizeRegsSeed=14")

// The by-val inline caches convert typed array elements through the macro assembler's FP temp register,
// so FTL must not keep a value there across them. Enough live doubles make the register allocator use it.

const liveDoubles = Array.from({ length: 30 }, (_, k) => `let d${k} = x * ${k + 1.25} + ${k};`).join("\n");
const liveList = Array.from({ length: 30 }, (_, k) => `d${k}`).join(", ");

function bodyFor(access)
{
    return `${liveDoubles}\n${access}\nreturn [result, ${liveList}];`;
}

function makeFunctions(name, parameters, access)
{
    let tested = new Function(...parameters, `// ${name}\n${bodyFor(access)}`);
    let reference = new Function(...parameters, `// ${name} reference\n${bodyFor(access)}`);
    noInline(tested);
    noDFG(reference);
    return { name, tested, reference };
}

function makeSuperMethods(name, prototype)
{
    let source = (label) => `({ __proto__: prototype, method(i, x) { // ${label}\n${bodyFor("let result = super[i];")} } })`;
    let tested = eval(source(name));
    let reference = eval(source(`${name} reference`));
    noInline(tested.method);
    noDFG(reference.method);
    return { name, tested, reference };
}

function shouldBeSame(name, iteration, actual, expected)
{
    for (let k = 0; k < expected.length; ++k) {
        if (!Object.is(actual[k], expected[k]))
            throw new Error(`${name}, iteration ${iteration}, slot ${k}: got ${actual[k]}, expected ${expected[k]}`);
    }
}

function makeReceivers()
{
    return [new Float64Array([1.5, 2.5]), new Float32Array([3.5, 4.5]), new Float16Array([5.5, 6.5]), [7.5, 8.5], { 0: 9.5, 1: 10.5 }];
}

const getByVal = makeFunctions("getByVal", ["a", "i", "x"], "let result = a[i];");
const putByVal = makeFunctions("putByVal", ["a", "i", "x"], "a[i] = x * 0.75 + 0.1; let result = 0;");
const superGets = [
    makeSuperMethods("getByValWithThis Float64Array", new Float64Array([11.5, 12.5])),
    makeSuperMethods("getByValWithThis Float32Array", new Float32Array([13.5, 14.5])),
    makeSuperMethods("getByValWithThis Float16Array", new Float16Array([15.5, 16.5])),
];

const testedReceivers = makeReceivers();
const referenceReceivers = makeReceivers();

for (let iteration = 0; iteration < 40000; ++iteration) {
    let index = iteration % testedReceivers.length;
    let i = iteration & 1;
    let x = (iteration % 1000) * 0.5;

    shouldBeSame(getByVal.name, iteration, getByVal.tested(testedReceivers[index], i, x), getByVal.reference(referenceReceivers[index], i, x));

    shouldBeSame(putByVal.name, iteration, putByVal.tested(testedReceivers[index], i, x), putByVal.reference(referenceReceivers[index], i, x));
    if (!Object.is(testedReceivers[index][i], referenceReceivers[index][i]))
        throw new Error(`putByVal, iteration ${iteration}: stored ${testedReceivers[index][i]}, expected ${referenceReceivers[index][i]}`);

    for (let { name, tested, reference } of superGets)
        shouldBeSame(name, iteration, tested.method(i, x), reference.method(i, x));
}
