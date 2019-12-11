function putByVal() {
    let proto = [0,1];
    let object = Object.create(proto);
    object[0] = 5;
}
noInline(putByVal);

function putById() {
    let proto = [0,1];
    let object = Object.create(proto);
    object.foo = 5;
}
noInline(putById);

for (let i = 0; i < 10000; i++) {
    putByVal();
    putById();
}
