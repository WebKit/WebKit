import { instantiate } from "../wabt-wrapper.js";

var object = { };
(async function () {
    try {
        object.wasmInstance = await instantiate(`
            (module
                (func (export "fence") (atomic.fence))
            )
            `, {}, { threads: true });

        var result = null;
        for (var i = 0; i < 1e8; ++i)
            result = object.wasmInstance.exports;
        if (typeof result.fence !== 'function')
            throw result;
    } catch (e) {
        print(String(e));
        $vm.abort();
    }
}());
