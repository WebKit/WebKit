function makeSteps(configuration)
{
    const steps = [];
    for (i = 0; i < configuration.stepCount; ++i) {
        steps.push(new BenchmarkTestStep(`Adding classes - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.addClasses(configuration.mutationsPerStep);
        }));
        steps.push(new BenchmarkTestStep(`Removing classes - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.removeClasses(configuration.mutationsPerStep);
        }));
        steps.push(new BenchmarkTestStep(`Mutating attributes - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.mutateAttributes(configuration.mutationsPerStep);
        }));
        steps.push(new BenchmarkTestStep(`Adding leaf elements - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.addLeafElements(configuration.mutationsPerStep);
        }));
        steps.push(new BenchmarkTestStep(`Removing leaf elements - ${i}`, (bench, contentWindow, contentDocument) => {
            bench.removeLeafElements(configuration.mutationsPerStep);
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
        tests: makeSteps(configuration),
    };
}

var Suites = [];
for (const configuration of StyleBench.predefinedConfigurations())
    Suites.push(makeSuite(configuration));
