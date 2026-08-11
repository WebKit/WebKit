let i = 10000;
let d = {
    get done() {
        if (i % 5000 == 0)
            i--;
        return !(--i);
    }
};

let x = {
    next: ()=>d
}

let iter = {};
iter[Symbol.iterator] = ()=>x;

for (let x of iter) {}
