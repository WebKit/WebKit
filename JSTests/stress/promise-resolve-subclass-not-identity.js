class MyPromise extends Promise {
    constructor(exec) { 
        super(exec); 
        this.inlineProp1 = 1;
    }
}
Object.defineProperty(MyPromise.prototype, 'constructor', { value: {} });

function test(p) {
    let dummy = p.inlineProp1; 
    return Promise.resolve(p);
}
noInline(test);

let prom = new MyPromise((r) => r());

for (let i = 0; i < 10000; i++) {
    test(prom);
}

let result = test(prom);
if (result === prom) {
    throw new Error("Promise.resolve should not be identity for subclasses");
}
