function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(async function () {
  let bytes0 = readFile('./resources/init-expr-ref-null-function-index-space-for-validation.wasm', 'binary');
  await WebAssembly.instantiate(bytes0, {});
})().catch(e => {
    if (String(e) !== `TypeError: import x:f must be an object`) {
        debug(String(e));
        $vm.abort();
    }
});
