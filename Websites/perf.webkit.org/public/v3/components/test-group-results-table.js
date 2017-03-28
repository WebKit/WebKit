
class TestGroupResultsTable extends ResultsTable {
    constructor()
    {
        super('test-group-results-table');
        this._testGroup = null;
        this._renderTestGroupLazily = new LazilyEvaluatedFunction(this._renderTestGroup.bind(this));
    }

    setTestGroup(testGroup)
    {
        this._testGroup = testGroup;
        this.enqueueToRender();
    }

    render()
    {
        super.render();
        this._renderTestGroupLazily.evaluate(this._testGroup, this._analysisResultsView);
    }

    _renderTestGroup(testGroup, analysisResults)
    {
        if (!analysisResults)
            return;
        const rowGroups = this._buildRowGroups();
        this.renderTable(
            analysisResults.metric().makeFormatter(4),
            rowGroups,
            'Configuration');
    }

    _buildRowGroups()
    {
        const testGroup = this._testGroup;
        if (!testGroup)
            return [];

        const commitSets = this._testGroup.requestedCommitSets();
        const resultsByCommitSet = new Map;
        const groups = commitSets.map((commitSet) => {
            const group = this._buildRowGroupForCommitSet(testGroup, commitSet, resultsByCommitSet);
            resultsByCommitSet.set(commitSet, group.results);
            return group;
        });

        const comparisonRows = [];
        for (let i = 0; i < commitSets.length - 1; i++) {
            const startCommit = commitSets[i];
            for (let j = i + 1; j < commitSets.length; j++) {
                const endCommit = commitSets[j];
                const startResults = resultsByCommitSet.get(startCommit) || [];
                const endResults = resultsByCommitSet.get(endCommit) || [];
                const row = this._buildComparisonRow(testGroup, startCommit, startResults, endCommit, endResults);
                if (!row)
                    continue;
                comparisonRows.push(row);
            }
        }

        groups.unshift({heading: '', rows: comparisonRows});

        return groups;
    }

    _buildRowGroupForCommitSet(testGroup, commitSet)
    {
        const rows = [new ResultsTableRow('Mean', commitSet)];
        const results = [];

        for (const request of testGroup.requestsForCommitSet(commitSet)) {
            const result = this._analysisResultsView.resultForBuildId(request.buildId());
            // Call result.commitSet() for each result since the set of revisions used in testing maybe different from requested ones.
            const row = new ResultsTableRow(1 + +request.order(), result ? result.commitSet() : null);
            rows.push(row);
            if (result) {
                row.setLink(result.build().url(), result.build().label());
                row.setResult(result);
                results.push(result);
            } else
                row.setLink(request.statusUrl(), request.statusLabel());
        }

        const aggregatedResult = MeasurementAdaptor.aggregateAnalysisResults(results);
        if (!isNaN(aggregatedResult.value))
            rows[0].setResult(aggregatedResult);

        return {heading: testGroup.labelForCommitSet(commitSet), rows, results};
    }

    _buildComparisonRow(testGroup, startCommit, startResults, endCommit, endResults)
    {
        const startConfig = testGroup.labelForCommitSet(startCommit);
        const endConfig = testGroup.labelForCommitSet(endCommit);

        const result = this._testGroup.compareTestResults(
            startResults.map((result) => result.value), endResults.map((result) => result.value));
        if (result.changeType == null)
            return null;

        const row = new ResultsTableRow(`${startConfig} to ${endConfig}`, null);
        const element = ComponentBase.createElement;
        row.setLabelForWholeRow(element('span',
            {class: 'results-label ' + result.status}, `${endConfig} is ${result.fullLabel} than ${startConfig}`));
        return row;
    }

    static cssTemplate()
    {
        return super.cssTemplate() + `
            .results-label {
                padding: 0.1rem;
                width: 100%;
                height: 100%;
            }

            th {
                vertical-align: top;
            }

            .failed {
                color: rgb(128, 51, 128);
            }
            .unchanged {
                color: rgb(128, 128, 128);
            }
            .worse {
                color: rgb(255, 102, 102);
                font-weight: bold;
            }
            .better {
                color: rgb(102, 102, 255);
                font-weight: bold;
            }
        `;
    }
}

ComponentBase.defineElement('test-group-results-table', TestGroupResultsTable);
