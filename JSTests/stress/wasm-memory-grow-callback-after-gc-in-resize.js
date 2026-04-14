function trigger() {
    let fillers = [];
    (function fill() {
        while (true) {
            try {
                fillers.push(new WebAssembly.Memory({initial: 65535, maximum: 65536}));
            } catch (e) {
                break;
            }
        }
    })();

    let buf;
    (function setup() {
        let mem = new WebAssembly.Memory({initial: 1, maximum: 65536});
        buf = mem.toResizableBuffer();
        mem = null;
    })();

    function clobber(n) {
        let a0=n|0,a1=n|0,a2=n|0,a3=n|0,a4=n|0,a5=n|0,a6=n|0,a7=n|0;
        let a8=n|0,a9=n|0,aa=n|0,ab=n|0,ac=n|0,ad=n|0,ae=n|0,af=n|0;
        let b0=n|0,b1=n|0,b2=n|0,b3=n|0,b4=n|0,b5=n|0,b6=n|0,b7=n|0;
        let b8=n|0,b9=n|0,ba=n|0,bb=n|0,bc=n|0,bd=n|0,be=n|0,bf=n|0;
        if (n > 0)
            return clobber(n - 1) + a0+a1+a2+a3+a4+a5+a6+a7+a8+a9+aa+ab+ac+ad+ae+af
                                + b0+b1+b2+b3+b4+b5+b6+b7+b8+b9+ba+bb+bc+bd+be+bf;
        return 0;
    }
    clobber(200);

    fillers = null;

    buf.resize(65536 * 65536);
}

function main() {
    for (let i = 0; i < 100; i++) {
        trigger();
    }
}

try {
    main();
} catch (e) {
    // May OOM in certain configurations
}
