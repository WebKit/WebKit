function foo(o, value) {
    try {
        throw { o: 1 };
    } catch({ o }){
        if (o !== 1)
            throw new Error();
        o = 2;
    }
    if (o !== value)
        throw new Error();

    try {
        throw [ 1 ];
    } catch([ o ]){
        if (o !== 1)
            throw new Error();
        o = 2;
    }
    if (o !== value)
        throw new Error();
}
foo("string", "string");

function bar(value) {
    let o = value;
    try {
        throw { o: 1 };
    } catch({ o }){
        if (o !== 1)
            throw new Error();
        o = 2;
    }
    if (o !== value)
        throw new Error();

    try {
        throw [ 1 ];
    } catch([ o ]){
        if (o !== 1)
            throw new Error();
        o = 2;
    }
    if (o !== value)
        throw new Error();
}
bar("string");

function bar(value) {
    const o = value;
    try {
        throw { o: 1 };
    } catch({ o }){
        if (o !== 1)
            throw new Error();
        o = 2;
    }
    if (o !== value)
        throw new Error();

    try {
        throw [ 1 ];
    } catch([ o ]){
        if (o !== 1)
            throw new Error();
        o = 2;
    }
    if (o !== value)
        throw new Error();
}
