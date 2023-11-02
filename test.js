function bb(x) { return x }
noInline(bb)

let obj = bb({})

obj.a = 1
obj.b = 12
obj.c = 13
obj.d = 14
obj.e = 15
obj.f = 16
obj.g = 17
obj.h = 18
obj.i = 19
obj.j = 2

// obj = Object.freeze(obj)

let x = {}

function test(o) {
    x = Object.assign({}, o);
    if (o == obj && x.j != 2)
        throw "Error: " + x
}

for (let i = 0; i < 1000; ++i) {
    test("")
    test(obj)
}

$vm.dumpCell(obj)
print(JSON.stringify(obj))
$vm.dumpCell(x)
print(JSON.stringify(x))
