let i = 10000;
let d = {
    get done() {
        if (i % 5000 == 0)
            OSRExit();
        return !(--i);
    },

    get value() { return i; }
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
