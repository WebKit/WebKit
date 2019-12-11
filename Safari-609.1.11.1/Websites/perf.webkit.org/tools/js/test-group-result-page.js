class TestGroupResultPage extends MarkupPage {
    constructor(title)
    {
        super(title);
        this._testGroup = null;
        this._analysisResults = null;
        this._analysisURL = null;
        this._analysisTask = null;
        this._constructTablesLazily = new LazilyEvaluatedFunction(this.constructTables.bind(this));
    }

    async setTestGroup(testGroup)
    {
        this._testGroup = testGroup;
        this._analysisTask = await testGroup.fetchTask();
        this._analysisResults = await AnalysisResults.fetch(this._analysisTask.id());
        this._analysisURL = TestGroupResultPage._urlForAnalysisTask(this._analysisTask);
        this.enqueueToRender();
    }

    static _urlForAnalysisTask(analysisTask)
    {
        return global.RemoteAPI.url(`/v3/#/analysis/task/${analysisTask.id()}`);
    }

    _resultsForTestGroup(testGroup, analysisResultsView)
    {
        const resultsByCommitSet = new Map;
        let maxValue = -Infinity;
        let minValue = Infinity;
        for (const commitSet of testGroup.requestedCommitSets())
        {
            const buildRequestsForCommitSet = testGroup.requestsForCommitSet(commitSet);
            const buildTypeRequests = buildRequestsForCommitSet.filter((buildRequest) => buildRequest.isBuild());
            const testTypeRequests = buildRequestsForCommitSet.filter((buildRequest) => buildRequest.isTest());

            const buildTypeResults = buildTypeRequests.map((buildRequest) => ({isBuild: true, hasCompleted: buildRequest.hasCompleted()}));
            const results = [...buildTypeResults, ...testTypeRequests.map((buildRequest) => analysisResultsView.resultForRequest(buildRequest))];
            resultsByCommitSet.set(commitSet, results);
            for (const result of results) {
                if (!result || result.isBuild)
                    continue;
                maxValue = Math.max(maxValue, result.value);
                minValue = Math.min(minValue, result.value);
            }
        }
        const diff = maxValue - minValue;
        minValue -= diff * 0.1;
        maxValue += diff * 0.1;

        return {resultsByCommitSet, widthForValue: (value) => (value - minValue) / (maxValue - minValue) * 100};
    }

    constructTables(testGroup, analysisResults, analysisURL, analysisTask)
    {
        const requestedCommitSets = testGroup.requestedCommitSets();
        console.assert(requestedCommitSets.length, 2);

        const metrics = analysisTask.metric() ? [analysisTask.metric()] : testGroup.test().metrics();

        const tablesWithSummary = metrics.map((metric) => this._constructTableForMetric(metric, testGroup, analysisResults, requestedCommitSets));
        const description = this.createElement('h1', [this.createElement('em', testGroup.name()), ' - ', this.createElement('em', this.createLink(analysisTask.name(), analysisURL))]);

        return [description, tablesWithSummary];
    }

    _constructTableForMetric(metric, testGroup, analysisResults, requestedCommitSets)
    {
        const formatter = metric.makeFormatter(4);
        const deltaFormatter = metric.makeFormatter(2, false);
        const formatValue = (value, interval) => {
            const delta = interval ? (interval[1] - interval[0]) / 2 : null;
            const resultParts = [value == null || isNaN(value) ? '-' : formatter(value)];
            if (delta != null && !isNaN(delta))
                resultParts.push(` \u00b1 ${deltaFormatter(delta)}`);
            return resultParts;
        };

        const analysisResultsView = analysisResults.viewForMetric(metric);
        const {resultsByCommitSet, widthForValue} = this._resultsForTestGroup(testGroup, analysisResultsView);

        const tableBodies = [];

        const beforeResults = resultsByCommitSet.get(requestedCommitSets[0]).filter((result) => !!result && !result.isBuild);
        const afterResults = resultsByCommitSet.get(requestedCommitSets[1]).filter((result) => !!result && !result.isBuild);
        const comparison = testGroup.compareTestResults(metric, beforeResults, afterResults);
        const changeStyleClassForMean = `${comparison.isStatisticallySignificantForMean ? comparison.changeType : 'insignificant'}-result`;
        const changeStyleClassForIndividual = `${comparison.isStatisticallySignificantForIndividual ? comparison.changeType : 'insignificant'}-result`;
        const caption = this.createElement('caption', `${testGroup.test().name()} - ${metric.aggregatorLabel()}`);

        tableBodies.push(this.createElement('tbody', {class: 'comparision-table-body'}, [
            this.createElement('tr', [this.createElement('td', 'Comparision by Mean'),
                this.createElement('td', this.createElement('em', {class: changeStyleClassForMean}, comparison.fullLabelForMean))]),
            this.createElement('tr', [this.createElement('td', 'Comparision by Individual'),
                this.createElement('td', this.createElement('em', {class: changeStyleClassForIndividual}, comparison.fullLabelForIndividual))])
        ]));

        for (const commitSet of requestedCommitSets) {
            let firstRow = true;
            const tableRows = [];
            const results = resultsByCommitSet.get(commitSet);
            const values = results.filter((result) => !!result).map((result) => result.value);
            const averageColumnContents = formatValue(Statistics.mean(values), Statistics.confidenceInterval(values));
            const label = testGroup.labelForCommitSet(commitSet);

            for (const result of results) {
                let cellValue = null;
                let barWidth = 0;
                if (!result)
                    cellValue = 'Failed';
                else if (result.isBuild)
                    cellValue = 'Build ' + (result.hasCompleted ? 'completed' : 'failed');
                else {
                    cellValue = formatValue(result.value, result.interval).join('');
                    barWidth = widthForValue(result.value);
                }
                tableRows.push(this._constructTableRow(cellValue, barWidth, firstRow, results.length, label, averageColumnContents));
                firstRow = false;
            }
            tableBodies.push(this.createElement('tbody', tableRows));
        }

        return this.createElement('table', {class: 'result-table'}, [caption, tableBodies]);
    }

    _constructTableRow(cellValue, barWidth, firstRow, tableHeadRowSpan, labelForCommitSet, averageColumnContents)
    {
        const barGraph = new BarGraph;
        barGraph.setWidth(barWidth);
        const cellContent = [barGraph, cellValue];

        if (firstRow) {
            return this.createElement('tr', [
                this.createElement('th', {class: 'first-row', rowspan: tableHeadRowSpan},
                    [labelForCommitSet + ': ', averageColumnContents.map((content => this.createElement('span', {class: 'no-wrap'}, content)))]),
                this.createElement('td', {class: 'result-cell first-row'}, cellContent),
            ])
        }
        else
            return this.createElement('tr', this.createElement('td', {class: 'result-cell'}, cellContent));
    }

    render()
    {
        super.render();
        this.renderReplace(this.content(), this._constructTablesLazily.evaluate(this._testGroup, this._analysisResults, this._analysisURL, this._analysisTask));
    }

    static get pageContent()
    {
        return [];
    }

    static get styleTemplate()
    {
        return {
            'body': {
                'font-family': 'sans-serif',
            },
            'h1': {
                'font-size': '1.3rem',
                'font-weight': 'normal',
            },
            'em': {
                'font-weight': 'bold',
                'font-style': 'normal',
                'padding-right': '2rem',
            },
            'caption': {
                'font-size': '1.3rem',
                'margin': '1rem 0',
                'text-align': 'left',
                'white-space': 'nowrap',
            },
            'td': {
                'padding': '0.2rem',
            },
            '.first-row': {
                'border-top': 'solid 1px #ccc',
            },
            '.no-wrap': {
                'white-space': 'nowrap',
            },
            'th': {
                'padding': '0.2rem',
            },
            '.result-table': {
                'margin-top': '1rem',
                'text-align': 'center',
                'border-collapse': 'collapse',
            },
            '.comparision-table-body': {
                'text-align': 'left',
            },
            '.result-cell': {
                'min-width': '20rem',
                'position': 'relative',
            },
            '.worse-result': {
                'color': '#c33',
            },
            '.better-result': {
                'color': '#33c',
            },
            '.insignificant-result': {
                'color': '#666',
            },
        }
    }
}

class BarGraph extends MarkupComponentBase {
    constructor()
    {
        super('bar-graph');
        this._constructBarGraphLazily = new LazilyEvaluatedFunction(this._constructBarGraph.bind(this));
    }

    setWidth(width)
    {
        this._width = width;
        this.enqueueToRender();
    }

    render()
    {
        super.render();
        this.renderReplace(this.content(), this._constructBarGraphLazily.evaluate(this._width));
    }

    _constructBarGraph(width)
    {
        const barGraphPlaceholder = this.createElement('div',{class: 'bar-graph-placeholder'});
        if (width)
            barGraphPlaceholder.style.width = width + '%';
        return barGraphPlaceholder;
    }

    static get contentTemplate()
    {
        return [];
    }

    static get styleTemplate()
    {
        return {
            ':host': {
                'position': 'absolute',
                'left': 0,
                'top': 0,
                'width': 'calc(100% - 2px)',
                'height': 'calc(100% - 2px)',
                'padding': '1px',
                'z-index': -1,
            },
            '.bar-graph-placeholder': {
                'background-color': '#ccc',
                'height': '100%',
                'width': '0rem',
            }
        };
    }
}

MarkupComponentBase.defineElement('test-group-result-page', TestGroupResultPage);
MarkupComponentBase.defineElement('bar-graph', BarGraph);


if (typeof module !== 'undefined')
    module.exports.TestGroupResultPage = TestGroupResultPage;