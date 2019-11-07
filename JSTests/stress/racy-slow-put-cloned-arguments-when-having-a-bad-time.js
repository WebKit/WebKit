//@ requireOptions("--jitPolicyScale=0")
//@ skip if ["arm", "mips"].include?($architecture)
//@ slow!

for (let j = 0; j < 500; j++) {
    runString(`
        function test() {
            "use strict";
            return arguments;
        }
        noInline(test);
        for (var i = 0; i < 10; i++)
            test();
        Object.defineProperty(Object.prototype, 0, {});
        test();
    `);
}
