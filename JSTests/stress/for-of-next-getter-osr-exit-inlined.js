let i = 0;
let x = {
    get next() {
        if (i++ === 8000)
                OSRExit();
        return () => { return { done: true }; };
    },
};
let iter = {};
iter[Symbol.iterator] = ()=>x;

function foo() {
    for (let x of iter) { }
}
noInline(foo);

for (let j = 0; j < 1e5; j++) {
    if (i !== j)
        throw new Error(i + ", " + j);
    foo();
}
