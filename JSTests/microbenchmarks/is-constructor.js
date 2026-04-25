const isConstructor = $vm.createBuiltin("(function (c) { return @isConstructor(c); })");
noInline(isConstructor);

class Foo {}
function Bar() {}

for (let i = 0; i < 1e6; ++i) {
    isConstructor(Foo);
    isConstructor(Bar);
    isConstructor(Array);
    isConstructor(Promise);
    isConstructor(null);
}
