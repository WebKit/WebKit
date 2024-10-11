function FileLocation(t0, t1) {
    this.file = t0;
    this.offset = t1;
};

function test(b1) {
    let b = false;
    if (b1) {
        b = new FileLocation(0, 1);
    } else {
        b = new FileLocation(0, 0);
    }

    let res = b instanceof FileLocation;
    b = b instanceof FileLocation;
    if (res !== b)
        throw new Error("bad");
}

for (let i = 0; i < 1e4; i++) {
    test(i % 2 == 0);
}
