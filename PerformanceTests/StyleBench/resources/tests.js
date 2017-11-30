function makeSteps(count)
{
    let steps = [];
    for (let i = 0; i < count; ++i) {
        steps.push(new BenchmarkTestStep('Adding classes', (bench, contentWindow, contentDocument) => {
            bench.addClasses(100);
        }));
        steps.push(new BenchmarkTestStep('Removing classes', (bench, contentWindow, contentDocument) => {
            bench.removeClasses(100);
        }));
        steps.push(new BenchmarkTestStep('Adding leaf elements', (bench, contentWindow, contentDocument) => {
            bench.addLeafElements(100);
        }));
        steps.push(new BenchmarkTestStep('Removing leaf elements', (bench, contentWindow, contentDocument) => {
            bench.removeLeafElements(100);
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
        tests: makeSteps(5),
    };
}

var Suites = [];
for (const configuration of StyleBench.predefinedConfigurations())
    Suites.push(makeSuite(configuration));
