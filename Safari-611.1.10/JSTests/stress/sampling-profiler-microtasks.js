var abort = $vm.abort;

if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js", "caller relative");

    let tree = null;
    function testResults() {
        if (!tree)
            tree = makeTree();
        else
            updateCallingContextTree(tree);

        let result = doesTreeHaveStackTrace(tree, ["jar", "hello", "promiseReactionJob"], false);
        return result;
    }

    let o1 = {};
    let o2 = {};
    function jar(x) {
        for (let i = 0; i < 1000; i++) {
            o1[i] = i;
            o2[i] = i + o1[i];
            i++;
            i--;
        }
        return x;
    }
    noInline(jar)

    let numLoops = 0;
    function loop() {
        let counter = 0;
        const numPromises = 100;
        function jaz() {
            Promise.resolve(42).then(function hello(v1) {
                for (let i = 0; i < 100; i++)
                    jar();
                counter++;
                if (counter >= numPromises) {
                    numLoops++;
                    if (!testResults()) {
                        if (numLoops > 5)
                            abort();
                        else
                            loop();
                    }
                }
            });
        }

        for (let i = 0; i < numPromises; i++)
            jaz();
    }

    loop();
}
