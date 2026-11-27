function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function makeClass(tag) {
    return class {
        *values() { yield tag; yield tag + 1; }
        async asyncValue() { await 0; return tag; }
    };
}

function makeGenerator(tag) {
    return function* () { yield tag; };
}

function* withParameter(a) { yield a; yield a * 2; }

function* withArguments() { yield arguments.length; }

function* noCapture() { yield 1; yield 2; }

async function noCaptureAsync() { await 0; return 3; }

let asyncArrow = async () => { await 0; return 4; };

async function* asyncGenerator() { await 0; yield 5; }

class Base {
    get base() { return 6; }
}

class Derived extends Base {
    *superGenerator() { yield super.base; }
    async superAsync() { await 0; return super.base; }
}

let errors = [];
function expectAsync(promise, expected) {
    promise.then((value) => {
        if (value !== expected)
            errors.push(new Error('bad async value: ' + value + ' expected: ' + expected));
    }, (error) => errors.push(error));
}

let classes = [makeClass(10), makeClass(20)];
let generators = [makeGenerator(1), makeGenerator(2), makeGenerator(3)];
let derived = new Derived;

function test(iterations) {
    for (let i = 0; i < iterations; ++i) {
        let C = classes[i & 1];
        let tag = (i & 1) ? 20 : 10;
        let instance = new C;
        let a = instance.values();
        let b = instance.values();
        shouldBe(a.next().value, tag);
        shouldBe(b.next().value, tag);
        shouldBe(a.next().value, tag + 1);
        shouldBe(a.next().done, true);
        shouldBe(b.next().value, tag + 1);

        let g = generators[i % 3];
        shouldBe(g().next().value, (i % 3) + 1);

        let p = withParameter(i);
        let q = withParameter(i + 1);
        shouldBe(p.next().value, i);
        shouldBe(q.next().value, i + 1);
        shouldBe(p.next().value, i * 2);
        shouldBe(q.next().value, (i + 1) * 2);

        shouldBe(withArguments(1, 2, 3).next().value, 3);

        let n = noCapture();
        let m = noCapture();
        shouldBe(n.next().value, 1);
        shouldBe(m.next().value, 1);
        shouldBe(n.next().value, 2);
        shouldBe(n.next().done, true);
        shouldBe(m.next().value, 2);

        shouldBe(derived.superGenerator().next().value, 6);

        expectAsync(noCaptureAsync(), 3);
        expectAsync(asyncArrow(), 4);
        expectAsync(asyncGenerator().next().then((result) => result.value), 5);
        expectAsync(derived.superAsync(), 6);
        if (i % 100 === 0)
            expectAsync(instance.asyncValue(), tag);

        if (i % 1000 === 0)
            classes[i & 1] = makeClass(tag);
    }

    drainMicrotasks();
    if (errors.length)
        throw errors[0];
}

test(testLoopCount);

// Recompiling the wrappers links fresh body executables, so the bodies cached by the first run no
// longer match and must be replaced.
$vm.deleteAllCodeWhenIdle();
setTimeout(() => test(100), 0);
