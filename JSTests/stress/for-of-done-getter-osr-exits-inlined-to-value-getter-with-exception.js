let i = 10000;
let e;
let d = {
    get done() {
        let result = !(--i);
        if (i % 5000 == 0)
            OSRExit();
        return result;
    },

    get value() {
        if (i % 5000 == 0)
            throw e = new Error();
        return i;
    }
};

let x = {
    next: ()=>d
}

let iter = {};
iter[Symbol.iterator] = ()=>x;

function foo() {
    for (let x of iter) {
        if (x !== --oldI)
            throw new Error();
    }
}

let oldI = i;
try {
    foo();
} catch (error) {
    if (e !== error)
        throw error
}

if (!e)
    throw new Error();
