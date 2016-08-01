typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];


function call(thunk) {
    thunk();
}
noInline(call);

let name = ["map", "reduce"];
// First test with every access being neutered.
function test(constructor) {
    let array = new constructor(10);
    transferArrayBuffer(array.buffer);
    for (let i = 0; i < 10000; i++) {
        call(() => {
            if (array.map !== constructor.prototype.map)
                throw new Error();
        });
        call(() => {
            if (array[name[i % 2]] !== constructor.prototype[name[i % 2]])
                throw new Error();
        });
    }
}

for (constructor of typedArrays) {
    test(constructor);
}

// Next test with neutered after tier up.
function test(constructor) {
    let array = new constructor(10);
    let i = 0;
    fnId = () =>  {
        if (array.map !== constructor.prototype.map)
            throw new Error();
    };
    fnVal = () => {
        if (array[name[i % 2]] !== constructor.prototype[name[i % 2]])
            throw new Error();
    };
    for (; i < 10000; i++) {
        call(fnId);
        call(fnVal);
    }
    transferArrayBuffer(array.buffer);
    call(fnId);
    call(fnVal);
}
