function assert(b) {
    if (!b)
        throw new Error;
}

let reg = RegExp(/foo/g)
function doExec() {
    return reg.exec("-foo");
}
noInline(doExec)

for (let i = 0; i < 1000; ++i) {
    let r = doExec();
    if ((i % 2) === 0)
        assert(r[0] === "foo");
    else
        assert(r === null);
}

