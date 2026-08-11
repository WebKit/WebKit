// Verify that changing a custom setter to a Function doesn't cause any issues.

function overrideFunction() {
    let o = {};
    let customThingy = $vm.createCustomTestGetterSetter();
    o.__proto__ = customThingy;

    o.customFunction = function() {
        Object.defineProperty(customThingy, "customFunction", {
            value: 42
        });
    };
}
noInline(overrideFunction);

for (let i = 0; i < 1000; ++i) {
    overrideFunction();
}
