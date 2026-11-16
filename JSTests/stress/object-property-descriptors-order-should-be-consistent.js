function abc() {}
let s = JSON.stringify(Object.keys(Object.getOwnPropertyDescriptors(abc)));
if (s !== `["length","name","prototype"]`) {
    throw Error("wrong");
}

function test(f) {
    let result = null;
    let testConsistent = (x) => {
        if (result == null) {
            result = x;
        } else {
            if (result !== x) {
                throw Error("inconsistent");
            }
        }
    };

    function a() {}
    f(a);
    testConsistent(JSON.stringify(Object.keys(Object.getOwnPropertyDescriptors(a))));
    testConsistent(JSON.stringify(Object.keys(Object.getOwnPropertyDescriptors(a))));
    testConsistent(JSON.stringify(Object.keys(Object.getOwnPropertyDescriptors(a))));
}

test((_) => {});

test((a) => {return a.prototype});
test((a) => {return a.length});
test((a) => {return a.name});

test((a) => {delete a.prototype});
test((a) => {delete a.length});
test((a) => {delete a.name});

