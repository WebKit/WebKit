import { instantiate } from "../wabt-wrapper.js";
import * as assert from '../assert.js';

(async function () {
    try {
        let instance = await instantiate(`
            (module
                (func (export "fence") (atomic.fence))
                (func (export "1") (atomic.fence))
            )
            `, {}, { threads: true });
        assert.eq(Reflect.getPrototypeOf(instance.exports), null);
        assert.eq(instance.exports.fence.name, '0');
        assert.eq(instance.exports[1].name, '1');
        assert.truthy(Object.isFrozen(instance.exports));
    } catch (e) {
        print(String(e));
        throw e;
    }
}()).catch($vm.abort);
