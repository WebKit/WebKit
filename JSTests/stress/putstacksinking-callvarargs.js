//@ runDefault("--useConcurrentJIT=0")
var iter;
function main() {
    function opt(i) {
        function x() {
            return y(...[1]);
        }

        function y() {
            return z.apply(this, arguments);
        }

        function z() {
            if (iter > 10136) {
                if (x.arguments[2] != 1.3)
                    throw "Wrong value"
                return x.arguments;
            }
        }
        noInline(z);

        x(1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.21, 1.22);
    }

    noInline(opt);
    for (let i = 0; i < 20000; ++i) {
        iter = i;
        opt(i);
    }
}
noDFG(main);

main();