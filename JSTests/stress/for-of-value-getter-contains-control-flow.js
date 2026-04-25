let i = 10000;
let d = {
    get value() {
        if (undefined instanceof Object)
            return 1;
        return 42;
    },

    get done() {
        return !(--i);
    }
};

let x = {
    next: ()=>d
}

let iter = {};
iter[Symbol.iterator] = ()=>x;

for (let x of iter) {}
