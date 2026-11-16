//@ runDefault("--jitPolicyScale=0", "--useSamplingProfiler=1")

function test() {
    function getInstance(bytes) {
        let u8 = Uint8Array.from(bytes, x=>x.charCodeAt(0));
        let module = new WebAssembly.Module(u8.buffer);
        return new WebAssembly.Instance(module);
    }

    let webAsm = getInstance('\0asm\x01\0\0\0\x01\x8E\x80\x80\x80\0\x03`\0\x01\x7F`\0\x01\x7F`\x01\x7F\x01\x7F\x03\x88\x80\x80\x80\0\x07\0\0\0\x01\x01\x02\x02\x04\x85\x80\x80\x80\0\x01p\x01\x07\x07\x07\x91\x80\x80\x80\0\x02\x05callt\0\x05\x05callu\0\x06\t\x8D\x80\x80\x80\0\x01\0A\0\x0B\x07\0\x01\x02\x03\x04\0\x02\nÆ\x80\x80\x80\0\x07\x84\x80\x80\x80\0\0A\x01\x0B\x84\x80\x80\x80\0\0A\x02\x0B\x84\x80\x80\x80\0\0A\x03\x0B\x84\x80\x80\x80\0\0A\x04\x0B\x84\x80\x80\x80\0\0A\x05\x0B\x87\x80\x80\x80\0\0 \0\x11\0\0\x0B\x87\x80\x80\x80\0\0 \0\x11\x01\0\x0B');

    for (let j = 0; j < 1000; j++) {
        try {
            webAsm.exports.callt(-1);
        } catch(e) {}
    }

    samplingProfilerStackTraces();
}

if (this.WebAssembly)
    test();
