Array.prototype[1] = 5;

function arrayEq(a, b) {
    if (a.length !== b.length)
        throw new Error([a, "\n\n",  b]);

    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error([a, "\n\n",  b]);
    }
}


let obj = {};
arrayEq([1,2,3].concat(4), [1,2,3,4]);
arrayEq([1,2,3].concat(1.34), [1,2,3,1.34]);
arrayEq([1.35,2,3].concat(1.34), [1.35,2,3,1.34]);
arrayEq([1.35,2,3].concat(obj), [1.35,2,3,obj]);
arrayEq([1,2,3].concat(obj), [1,2,3,obj]);
