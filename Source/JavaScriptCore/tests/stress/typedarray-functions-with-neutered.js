typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

proto = Int8Array.prototype.__proto__;

function getGetter(prop) {
    return Object.getOwnPropertyDescriptor(proto, prop).get;
}

function unit() { }


prototypeFunctions = [
    { func:getGetter("length"), args:[], result:0 },
    { func:getGetter("byteLength"), args:[], result:0 },
    { func:getGetter("byteOffset"), args:[], result:0 },
    { func:proto.copyWithin, args:[0, 1] },
    { func:proto.entries, args:[] },
    { func:proto.every, args:[unit] },
    { func:proto.every, args:[1] },
    { func:proto.filter, args:[unit] },
    { func:proto.find, args:[] },
    { func:proto.findIndex, args:[] },
    { func:proto.forEach, args:[] },
    { func:proto.indexOf, args:[] },
    { func:proto.join, args:[] },
    { func:proto.keys, args:[] },
    { func:proto.lastIndexOf, args:[] },
    { func:proto.map, args:[] },
    { func:proto.reduce, args:[] },
    { func:proto.reduceRight, args:[] },
    { func:proto.reverse, args:[] },
    { func:proto.set, args:[[]] },
    { func:proto.set, args:[new Int32Array(1)] },
    { func:proto.set, args:[new Int32Array(1)] },
    { func:proto.set, args:[new Int32Array(1), -1], error:"RangeError: Offset should not be negative" },
    { func:proto.slice, args:[] },
    { func:proto.some, args:[] },
    { func:proto.sort, args:[] },
    { func:proto.subarray, args:[] },
    { func:proto.toString, args:[] },
    { func:proto.values, args:[] },
];

arrays = typedArrays.map(function(constructor) {
    let view = new constructor(10);
    transferArrayBuffer(view.buffer);
    return view;
});

function checkProtoFunc(testArgs) {
    function throwsCorrectError(elem) {
        try {
            result = testArgs.func.call(...[elem, ...testArgs.args]);
            if (testArgs.result !== undefined) {
                return result === testArgs.result;
            }
        } catch (e) {
            if (testArgs.error)
                return e == testArgs.error;
            return e == "TypeError: Underlying ArrayBuffer has been detached from the view";
        }
        return false;
    }

    if (!arrays.every(throwsCorrectError))
        throw "bad" + testArgs.func.name;
}

function test() {
    prototypeFunctions.forEach(checkProtoFunc);
}

for (var i = 0; i < 1000; i++)
    test();

