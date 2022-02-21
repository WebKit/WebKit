function assert(b) {
    if (!b)
        throw new Error;
}

function main() {
    let result;

    const v13 = [0, 0]; 
    Array.prototype[-80887344] = v13;

    const func = (i, ...rest) => {
        result = rest[i];
    };  
    noInline(func);

    for (let v30 = 0; v30 < 10000; v30++) {
        func(0);
    }

    func(-80887344);
    assert(result === v13);
}
noDFG(main);
main();
