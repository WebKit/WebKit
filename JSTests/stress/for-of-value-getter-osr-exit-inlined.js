let i = 10000;
let d = {
    get value() {
        if (i === 5000)
            OSRExit();
        return --i;
    },

    get done() { return !i; }
};

let x = {
    next: ()=>d
}

let iter = {};
iter[Symbol.iterator] = ()=>x;

let oldI = i;
for (let x of iter) {
    if (x !== --oldI)
        throw new Error();
}
