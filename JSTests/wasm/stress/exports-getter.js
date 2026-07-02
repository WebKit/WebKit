//@ $skipModes << "wasm-no-wasm-jit".to_sym if $buildType == "debug"
//@ $skipModes << "wasm-no-jit".to_sym if $buildType == "debug"

import { instantiate } from "../wabt-wrapper.js";

function bench(instance)
{
    var last = null;
    for (var i = 0; i < 1e8; ++i)
        last = instance.exports;
    return last;
}
noInline(bench);

(async function () {
    try {
        let instance = await instantiate(`
            (module
                (func (export "fence") (atomic.fence))
            )
            `, {}, { threads: true });

        let result = bench(instance);
        if (typeof result.fence !== 'function')
            throw result;
    } catch (e) {
        print(String(e));
        $vm.abort();
    }
}());
