//@ runDefault("--useTypeProfiler=true")

var findTypeForExpression = $vm.findTypeForExpression;

function wrapper(x) {
    class Base {
        constructor() {
            this['"'] = true;
        }
    };

    var baseInstance = new Base;
}
wrapper();

var types = findTypeForExpression(wrapper, "baseInstance = new Base");
JSON.stringify(types)
