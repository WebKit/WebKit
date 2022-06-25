//@ crashOK!
//@ runDefault("--useLLInt=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function dictionary() {
    var object = $vm.createObjectDoingSideEffectPutWithoutCorrectSlotStatus();
    for(var i = 0; i < 66; i++)
        object['p'+i] = i;
    return object;
}

function putter(o) {
    o._unsupported = not_string;
}

var d;
var not_string = {
    toString() {
        for(var i = 3; i < 66; i++)
            delete d['p'+i];
        for(i in Object.create(d));
        return null;
    }
};

for (var i = 0; i < 100; ++i) {
    d = dictionary();
    putter(d);
    shouldBe('p3' in d, false);
}

if ($vm.assertEnabled())
    throw new Error();
