function assert(b) {
    if (!b)
        throw new Error;
}

function main() {
    let result;

    const v13 = [0, 0]; 
    Array.prototype[-1] = v13;

    const func = function func(i, ...args) {
        result = args[i];
    };  
    noInline(func);

    for (let v30 = 0; v30 < 10000; v30++) {
        func(1000, 10);
    }   

    func(-1, 10);
    assert(result === v13);
}
noDFG(main);
main();
