describe('TestGroupResultPage', () => {

    async function importMarkupComponent(context)
    {
        return await context.importScripts(['lazily-evaluated-function.js', '../shared/common-component-base.js', '../../tools/js/markup-component.js', '../../tools/js/test-group-result-page.js',
            'models/data-model.js', 'models/metric.js', '../shared/statistics.js',], 'TestGroupResultPage', 'Metric');
    }

    async function prepareTestGroupResultPage(context, buildRequestsA, buildRequestsB)
    {
        const [TestGroupResultPage, Metric] = await importMarkupComponent(context);

        const mockAnalysisTask = {
            metric: () => ({
                makeFormatter: (sigFig, alwaysShowSign) => Metric.makeFormatter('MB', sigFig, alwaysShowSign),
                aggregatorLabel: () => 'Arithmetic mean'
            }),
            name: () => 'mock-analysis-task'
        };

        const mockTestGroup = {
            requestedCommitSets: () => ['A', 'B'],
            test: () => ({test: () => 'speedometer-2', name: () => 'speedometer-2'}),
            labelForCommitSet: (commitSet) => commitSet,
            requestsForCommitSet: (commitSet) => ({'A': buildRequestsA, 'B': buildRequestsB}[commitSet]),
            compareTestResults: (...args) => ({isStatisticallySignificant: true, changeType: 'worse'}),
            name: () => 'mock-test-group',
        };

        const mockAnalysisResults = {
            viewForMetric: (metric) => ({resultForRequest: (buildRequest) => (buildRequest.value === null ? null : {value: buildRequest.value})})
        };

        const page = new TestGroupResultPage('test');
        page._testGroup = mockTestGroup;
        page._analysisTask = mockAnalysisTask;
        page._analysisResults = mockAnalysisResults;
        page._analysisURL = 'http://localhost';

        return page;
    }

    function prepareBuildRequests(buildRequestValues, isTestBooleanList, hasCompletedBooleanList)
    {
        return Array.from(buildRequestValues.entries()).map((entry) => {
            const [index, value] = entry;
            const isTest = isTestBooleanList ? isTestBooleanList[index] : true;
            const hasCompleted = hasCompletedBooleanList ? hasCompletedBooleanList[index] : true;
            return {value, isTest: () => isTest, isBuild: () => !isTest, hasCompleted: () => hasCompleted};
        });
    }

    it('should render failed test group with empty bar', async () => {
        const context = new BrowsingContext();
        const page = await prepareTestGroupResultPage(context, prepareBuildRequests([null, 3, 5]), prepareBuildRequests([2, 4, 6]));
        await page.enqueueToRender();
        const document = context.document;
        document.open();
        document.write(page.generateMarkup());
        document.close();

        expect(context.global.getComputedStyle(context.document.querySelector('.bar-graph-placeholder')).width).to.be('0px');
    });

    it('should render right ratio based on test group result', async () => {
        const context = new BrowsingContext();
        const resultA = [1, 3, 5];
        const resultB = [2, 4, 6];
        const page = await prepareTestGroupResultPage(context, prepareBuildRequests(resultA), prepareBuildRequests(resultB));
        page.enqueueToRender();
        const document = context.document;
        document.open();
        document.write(page.generateMarkup());
        document.close();

        const min = 0.5;
        const max = 6.5;
        const expectedPercentages = [...resultA, ...resultB].map((result) => (result - min) / (max - min));
        const barNodes = context.document.querySelectorAll('.bar-graph-placeholder');
        expect(barNodes.length).to.be(6);

        const almostEqual = (a, b) => Math.abs(a - b) < 0.001;
        let previousNodeWidth = parseFloat(context.global.getComputedStyle(barNodes[0]).width);
        for (let i = 1; i < 6; ++ i) {
            const currentNodeWidth = parseFloat(context.global.getComputedStyle(barNodes[i]).width);
            expect(almostEqual(currentNodeWidth / previousNodeWidth, expectedPercentages[i] / expectedPercentages[i - 1])).to.be(true);
            previousNodeWidth = currentNodeWidth;
        }
    });

    it('should not render rows for build requests those are not test', async () => {
        const context = new BrowsingContext();
        const resultA = [null, 1, 3, 5];
        const isTestBooleanListA = [false, true, true, true];
        const isBuildBooleanListA = [true, true, true, true];
        const resultB = [null, 2, 4, 6];
        const isTestBooleanListB = [false, true, true, true];
        const isBuildBooleanListB = [false, true, true, true];
        const page = await prepareTestGroupResultPage(context,
            prepareBuildRequests(resultA, isTestBooleanListA, isBuildBooleanListA),
            prepareBuildRequests(resultB, isTestBooleanListB, isBuildBooleanListB));
        page.enqueueToRender();
        const document = context.document;
        document.open();
        document.write(page.generateMarkup());
        document.close();

        const barNodes = context.document.querySelectorAll('.result-cell');
        expect(barNodes.length).to.be(8);
        expect(barNodes[0].textContent).to.be('Build completed');
        expect(barNodes[4].textContent).to.be('Build failed');
    });
});