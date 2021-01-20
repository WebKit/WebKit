let value = "string";
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
