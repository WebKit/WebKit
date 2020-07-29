//@ skip if $architecture == "x86" || $buildType == "debug"

function assert(b) {
    if (!b)
        throw new Error("Bad!")
}
noInline(assert);

let calledGet = false;
let definedAccessor = false;
function test() {
    function foo(...rest) {
        return rest;
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        const size = 800;
        let arr = new Array(size);
        for (let i = 0; i < size; i++)
            arr[i] = i;
        let result = foo(...arr);

        assert(result.length === arr.length);
        assert(result.length === size);
        for (let i = 0; i < arr.length; i++) {
            assert(arr[i] === result[i]);
            assert(result[i] === i);
        }
        if (definedAccessor) {
            calledGet = false;
            result[0];
            assert(!calledGet);
            arr[0];
            assert(calledGet);

            let testArr = [...arr];
            calledGet = false;
            testArr[0];
            assert(!calledGet);
        }
    }
}
test();

definedAccessor = true;
Reflect.defineProperty(Array.prototype, "0", {
    get() { calledGet = true; return 0; },
    set(x) {  }
});
test();
