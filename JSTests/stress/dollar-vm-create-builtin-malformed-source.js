//@ requireOptions("--useDollarVM=1")

function shouldThrow(fn) {
    let threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (!(e instanceof TypeError))
            throw new Error("expected TypeError, got " + e);
    }
    if (!threw)
        throw new Error("expected to throw");
}

// rdar://175155846: BuiltinExecutables::createExecutable uses a hand-rolled parser that
// RELEASE_ASSERTs on input that does not match the exact shape produced by the builtins
// generator. $vm.createBuiltin must reject such input instead of crashing.

// Missing space between "function" and "(".
shouldThrow(() => $vm.createBuiltin('(function(o){return @getByIdDirectPrivate(o,"prototype")})'));

// Assorted malformed shapes.
shouldThrow(() => $vm.createBuiltin(''));
shouldThrow(() => $vm.createBuiltin('function (){}'));
shouldThrow(() => $vm.createBuiltin('(function )'));
shouldThrow(() => $vm.createBuiltin('(function (xxxxxxxxxxxxxxxxxx'));
shouldThrow(() => $vm.createBuiltin('(function (a,b)a+b)'));
shouldThrow(() => $vm.createBuiltin('(async function(){})'));
shouldThrow(() => $vm.createBuiltin('(function (\u{1F600}){})'));

// Well-formed shapes still work.
if (typeof $vm.createBuiltin('(function (a, b) { return a + b; })') !== "function")
    throw new Error("well-formed builtin should produce a function");
if (typeof $vm.createBuiltin('(async function (a) { return a; })') !== "function")
    throw new Error("well-formed async builtin should produce a function");
