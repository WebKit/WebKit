//@ runDefault("--useTypeProfiler=true")

var findTypeForExpression = $vm.findTypeForExpression;

function wrapper(x) {
    class Base {
        constructor() { }
    };

    var baseInstance = new Base;
    Base.displayName = '"';
}
wrapper();

var types = findTypeForExpression(wrapper, "baseInstance = new Base");
JSON.stringify(types)

