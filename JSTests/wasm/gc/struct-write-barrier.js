// requireOptions("--collectContinuously=1")

import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

let wat = `
    (module
        (type $Cell (struct (field i32)))
        (type $Wrapper (struct (field (mut (ref null $Cell)))))

        (func (export "init") (result (ref $Wrapper))
            (struct.new_default $Wrapper)
        )

        (func (export "set") (param (ref $Wrapper) i32)
            (local.get 0)
            (struct.new $Cell (local.get 1))
            (struct.set $Wrapper 0)
        )

        (func (export "get") (param (ref $Wrapper)) (result (ref null $Cell))
            (local.get 0)
            (struct.get $Wrapper 0)
        )
    )
`

let instance = await instantiate(wat, {});

let wrapper = instance.exports.init();

for (let i = 0; i < wasmTestLoopCount; ++i) {
    instance.exports.set(wrapper, i);
    instance.exports.get(wrapper);
}
