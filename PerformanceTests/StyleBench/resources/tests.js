function makeSteps(count)
{
    const steps = [];
    for (i = 0; i < count; ++i) {
        steps.push(new BenchmarkTestStep(`Adding classes - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.addClasses(25);
        }));
        steps.push(new BenchmarkTestStep(`Removing classes - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.removeClasses(25);
        }));
        steps.push(new BenchmarkTestStep(`Adding leaf elements - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.addLeafElements(25);
        }));
        steps.push(new BenchmarkTestStep(`Removing leaf elements - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.removeLeafElements(25);
        }));
    }
    return steps;
}

function makeSuite(configuration)
{
    return {
        name: configuration.name,
        url: 'style-bench.html',
        prepare: (runner, contentWindow, contentDocument) => {
            return runner.waitForElement('#testroot').then((element) => {
                return contentWindow.createBenchmark(configuration);
            });
        },
        tests: makeSteps(10),
    };
}

var Suites = [];
for (const configuration of StyleBench.predefinedConfigurations())
    Suites.push(makeSuite(configuration));
