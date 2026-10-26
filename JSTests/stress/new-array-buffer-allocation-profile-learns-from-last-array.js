//@ $skipModes << :lockdown
//@ requireOptions("--useArrayAllocationProfiling=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected: " + expected);
}

const int32 = "CopyOnWriteArrayWithInt32";
const double = $vm.allowDoubleShape() ? "CopyOnWriteArrayWithDouble" : "CopyOnWriteArrayWithContiguous";
const contiguous = "CopyOnWriteArrayWithContiguous";
const slowPut = "ArrayWithSlowPutArrayStorage";

function keeps(make, write, mode, contents) {
    noInline(make);
    noDFG(make);
    for (let i = 0; i < 8; ++i) {
        let array = make();
        shouldBe($vm.indexingMode(array), mode);
        shouldBe(JSON.stringify(array), contents);
        write(array);
    }
}

function learns(make, write, from, to, contents) {
    noInline(make);
    noDFG(make);
    for (let tries = 0; ; ++tries) {
        let array = make();
        shouldBe($vm.indexingMode(array), from);
        write(array);
        if ($vm.indexingMode(make()) === to)
            break;
        if (tries === 10)
            throw new Error("the site did not go from " + from + " to " + to);
    }
    keeps(make, () => { }, to, contents);
}

keeps(() => [1, 2, 3], () => { }, int32, "[1,2,3]");
keeps(() => [1, 2, 3], (array) => { array[0] = 7; }, int32, "[1,2,3]");
keeps(() => [1, 2, 3], (array) => { array.foo = 1; }, int32, "[1,2,3]");
keeps(() => [1], (array) => { for (let i = 0; i < 40; ++i) array.push(i); }, int32, "[1]");
keeps(() => [1.5, 2.5], () => { }, double, "[1.5,2.5]");
keeps(() => [1.5, 2.5], (array) => { array[0] = 3; }, double, "[1.5,2.5]");
keeps(() => ["a", "b"], () => { }, contiguous, '["a","b"]');
keeps(() => ["a", "b"], (array) => { array[0] = 1.5; }, contiguous, '["a","b"]');
keeps(() => ["a", "b"], (array) => { Object.freeze(array); }, contiguous, '["a","b"]');

learns(() => [1, 2, 3], (array) => { array[0] = "x"; }, int32, contiguous, "[1,2,3]");
learns(() => [1, 2, 3], (array) => { $vm.ensureArrayStorage(array); }, int32, contiguous, "[1,2,3]");
learns(() => [1, 2, 3], (array) => { Object.freeze(array); }, int32, contiguous, "[1,2,3]");

let twice = () => [1, 2, 3];
learns(twice, (array) => { array[0] = 1.5; }, int32, double, "[1,2,3]");
if (double !== contiguous)
    learns(twice, (array) => { array[1] = "x"; }, double, contiguous, "[1,2,3]");

let beforeBadTime = () => [1, 2, 3];
keeps(beforeBadTime, () => { }, int32, "[1,2,3]");
$vm.haveABadTime(globalThis);
keeps(beforeBadTime, () => { }, slowPut, "[1,2,3]");
keeps(() => [1, 2, 3], (array) => { array[0] = 1.5; }, slowPut, "[1,2,3]");
