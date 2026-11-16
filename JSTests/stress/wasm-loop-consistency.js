//@ requireOptions("--useExecutableAllocationFuzz=false")
// https://bugs.webkit.org/show_bug.cgi?id=226012
if ($vm.isWasmSupported()) {
    // (module
    // (type (;0;) (func (param i32)))
    // (func (;0;) (type 0) (param i32)
    //   local.get 0
    //   local.get 0
    //   loop (param i32)  ;; label = @1
    //     drop
    //   end
    //   drop))
    var wasmCode = new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 1, 127, 0, 3, 2, 1, 0, 10, 13, 1, 11, 0, 0x20, 0, 0x20, 0, 0x3, 0, 0x1A, 0xb, 0x1A, 0xb]));
}

