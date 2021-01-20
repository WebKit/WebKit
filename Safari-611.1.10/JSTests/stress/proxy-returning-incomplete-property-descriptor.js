// This test should not crash.
function truthiness(x) {
    return !!x;
}

function compare(a, b) {
    for (var i in a.desc) {
        let propA = a.desc[i];
        let propB = b.desc[i];
        if (propA == propB)
            continue;
        if (typeof propA == "boolean" && truthiness(propA) == truthiness(propB))
            continue;
        throw Error(a.name + "[" + i + "] : " + propA + " != " + b.name + "[" + i + "] : " + propB);
    }
}

function shouldBe(actualDesc, expectedDesc) {
    compare({ name: "actual", desc: actualDesc }, { name: "expected", desc: expectedDesc });
    compare({ name: "expected", desc: expectedDesc }, { name: "actual", desc: actualDesc });
}

function test(expectedDesc) {
    var desc = Object.getOwnPropertyDescriptor(new Proxy({a:0}, {
            getOwnPropertyDescriptor(t,pk) {
                return expectedDesc
            }
        }), "");
    shouldBe(desc, expectedDesc);
}

test({ configurable:true });
test({ writable:true, configurable:true });
test({ writable:true, enumerable:true, configurable:true });
test({ enumerable:true, configurable:true, get: function() {} });
test({ enumerable:true, configurable:true, set: function() {} });
