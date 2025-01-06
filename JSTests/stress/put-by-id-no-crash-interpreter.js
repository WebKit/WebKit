//@ runDefault("--useJIT=0")
const propName = "myProp";
var d;

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error("Failure, expected: " + expected + ", got: " + actual);
}

function createDictionary()
{
    var object = $vm.createObjectDoingSideEffectPutWithoutCorrectSlotStatus();
    object[propName] = 42;
    return object;
}

function putWithSideEffects(o)
{
    o.dummy = {
        toString() {
            delete d[propName];
            return "foo";
        }
    };
}

d = createDictionary();
putWithSideEffects(d);
shouldBe(propName in d, false);
