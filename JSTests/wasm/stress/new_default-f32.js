function instantiate(moduleBase64, importObject, compileOptions) {
    let bytes = Uint8Array.fromBase64(moduleBase64);
    return WebAssembly.instantiate(bytes, importObject, compileOptions);
}

(async function() {
    /*
        (module
            (type (struct (field f32)))
            (global (mut f32) f32.const 0)
            (func (export "fn")
                struct.new_default 0
                struct.get 0 0
                global.set 0
            )
        )
    */
    let { instance } = await instantiate("AGFzbQEAAAABCAJfAX0AYAAAAwIBAQYJAX0BQwAAAAALBwYBAmZuAAAKDQELAPsBAPsCAAAkAAs=");
    let { fn } = instance.exports;
    for (let i = 0; i < wasmTestLoopCount; ++i)
        fn();
})().then(async function() {
    /*
        (module
            (type (array f32))
            (global (mut f32) f32.const 0)
            (func (export "fn")
                i32.const 1
                array.new_default 0
                i32.const 0
                array.get 0
                global.set 0
            )
        )
    */
    let { instance } = await instantiate("AGFzbQEAAAABBwJefQBgAAADAgEBBgkBfQFDAAAAAAsHBgECZm4AAAoQAQ4AQQH7BwBBAPsLACQACw==");
    let { fn } = instance.exports;
    for (let i = 0; i < wasmTestLoopCount; ++i)
        fn();
}).catch(e => {
    print("error:", e.constructor.name, e.message);
    if (e.stack)
        report(e.stack);
});
