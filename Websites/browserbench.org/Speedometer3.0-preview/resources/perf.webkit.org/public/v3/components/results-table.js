class ResultsTable extends ComponentBase {
    constructor(name)
    {
        super(name);
        this._repositoryList = [];
        this._analysisResultsView = null;
    }

    setAnalysisResultsView(analysisResultsView)
    {
        console.assert(analysisResultsView instanceof AnalysisResultsView);
        this._analysisResultsView = analysisResultsView;
        this.enqueueToRender();
    }

    renderTable(valueFormatter, rowGroups, headingLabel, buildHeaders = (headers) => headers, buildColumns = (columns, row, rowIndex) => columns)
    {
        Instrumentation.startMeasuringTime('ResultsTable', 'renderTable');

        const [repositoryList, constantCommits] = this._computeRepositoryList(rowGroups);

        const barGraphGroup = new BarGraphGroup();
        barGraphGroup.setShowLabels(true);
        const element = ComponentBase.createElement;
        let hasGroupHeading = false;
        const tableBodies = rowGroups.map((group) => {
            const groupHeading = group.heading;
            const revisionSupressionCount = {};
            hasGroupHeading = hasGroupHeading || groupHeading;

            return element('tbody', group.rows.map((row, rowIndex) => {
                const cells = [];

                if (groupHeading !== undefined && !rowIndex)
                    cells.push(element('th', {rowspan: group.rows.length}, groupHeading));
                cells.push(element('td', row.heading()));

                if (row.labelForWholeRow())
                    cells.push(element('td', {class: 'whole-row-label', colspan: repositoryList.length + 1}, row.labelForWholeRow()));
                else {
                    cells.push(element('td', row.resultContent(valueFormatter, barGraphGroup)));
                    cells.push(this._createRevisionListCells(repositoryList, revisionSupressionCount, group, row.commitSet(), rowIndex));
                }

                return element('tr', buildColumns(cells, row, rowIndex));
            }));
        });

        this.renderReplace(this.content().querySelector('table'), [
            tableBodies.length ? element('thead', [
                buildHeaders([
                    ComponentBase.createElement('th', {colspan: hasGroupHeading ? 2 : 1}, headingLabel),
                    element('th', 'Result'),
                    repositoryList.map((repository) => element('th', repository.label())),
                ]),
            ]) : [],
            tableBodies,
        ]);

        this.renderReplace(this.content('constant-commits'), constantCommits.map((commit) => element('li', commit.title())));

        Instrumentation.endMeasuringTime('ResultsTable', 'renderTable');
    }

    _createRevisionListCells(repositoryList, revisionSupressionCount, testGroup, commitSet, rowIndex)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        const cells = [];
        for (const repository of repositoryList) {
            const commit = commitSet ? commitSet.commitForRepository(repository) : null;

            if (revisionSupressionCount[repository.id()]) {
                revisionSupressionCount[repository.id()]--;
                continue;
            }

            let succeedingRowIndex = rowIndex + 1;
            while (succeedingRowIndex < testGroup.rows.length) {
                const succeedingCommitSet = testGroup.rows[succeedingRowIndex].commitSet();
                if (succeedingCommitSet && commit != succeedingCommitSet.commitForRepository(repository))
                    break;
                succeedingRowIndex++;
            }
            const rowSpan = succeedingRowIndex - rowIndex;
            const attributes = {class: 'revision'};
            if (rowSpan > 1) {
                revisionSupressionCount[repository.id()] = rowSpan - 1;
                attributes['rowspan'] = rowSpan;                       
            }
            if (rowIndex + rowSpan >= testGroup.rows.length)
                attributes['class'] += ' lastRevision';

            let content = 'Missing';
            if (commit) {
                const url = commit.url();
                content = url ? link(commit.label(), url) : commit.label();
            }

            cells.push(element('td', attributes, content));
        }
        return cells;
    }

    _computeRepositoryList(rowGroups)
    {
        const allRepositories = Repository.sortByNamePreferringOnesWithURL(Repository.all());
        const commitSets = [];
        for (let group of rowGroups) {
            for (let row of group.rows) {
                const commitSet = row.commitSet();
                if (commitSet)
                    commitSets.push(commitSet);
            }
        }
        if (!commitSets.length)
            return [[], []];

        const changedRepositorySet = new Set;
        const constantCommits = new Set;
        for (let repository of allRepositories) {
            const someCommit = commitSets[0].commitForRepository(repository);
            if (CommitSet.containsMultipleCommitsForRepository(commitSets, repository))
                changedRepositorySet.add(repository);
            else if (someCommit)
                constantCommits.add(someCommit);
        }
        return [allRepositories.filter((repository) => changedRepositorySet.has(repository)), [...constantCommits]];
    }

    static htmlTemplate()
    {
        return `<table class="results-table"></table><ul id="constant-commits"></ul>`;
    }

    static cssTemplate()
    {
        return `
            table.results-table {
                border-collapse: collapse;
                border: solid 0px #ccc;
                font-size: 0.9rem;
            }

            .results-table th {
                text-align: center;
                font-weight: inherit;
                font-size: 1rem;
                padding: 0.2rem;
            }

            .results-table td {
                height: 1.4rem;
                text-align: center;
            }

            .results-table td.whole-row-label {
                text-align: left;
            }

            .results-table thead {
                color: #c93;
            }

            .results-table td.revision {
                vertical-align: top;
                position: relative;
            }

            .results-table tr:not(:last-child) td.revision:after {
                display: block;
                content: '';
                position: absolute;
                left: calc(50% - 0.25rem);
                bottom: -0.4rem;
                border-style: solid;
                border-width: 0.25rem;
                border-color: #999 transparent transparent transparent;
            }

            .results-table tr:not(:last-child) td.revision:before {
                display: block;
                content: '';
                position: absolute;
                left: calc(50% - 1px);
                top: 1.2rem;
                height: calc(100% - 1.4rem);
                border: solid 1px #999;
            }
            
            .results-table tr:not(:last-child) td.revision.lastRevision:after {
                bottom: -0.2rem;
            }
            .results-table tr:not(:last-child) td.revision.lastRevision:before {
                height: calc(100% - 1.6rem);
            }

            .results-table tbody:not(:first-child) tr:first-child th,
            .results-table tbody:not(:first-child) tr:first-child td {
                border-top: solid 1px #ccc;
            }

            .results-table bar-graph {
                display: block;
                width: 7rem;
                height: 1rem;
            }

            #constant-commits {
                list-style: none;
                margin: 0;
                padding: 0.5rem 0 0 0.5rem;
                font-size: 0.8rem;
            }

            #constant-commits:empty {
                padding: 0;
            }

            #constant-commits li {
                display: inline;
            }

            #constant-commits li:not(:last-child):after {
                content: ', ';
            }
        `;
    }
}

class ResultsTableRow {
    constructor(heading, commitSet)
    {
        this._heading = heading;
        this._result = null;
        this._link = null;
        this._label = '-';
        this._commitSet = commitSet;
        this._labelForWholeRow = null;
    }

    heading() { return this._heading; }
    commitSet() { return this._commitSet; }
    setResult(result) { this._result = result; }
    setLink(link, label)
    {
        this._link = link;
        this._label = label;
    }

    setLabelForWholeRow(label) { this._labelForWholeRow = label; }
    labelForWholeRow() { return this._labelForWholeRow; }

    resultContent(valueFormatter, barGraphGroup)
    {
        let resultContent = this._label;
        if (this._result) {
            const value = this._result.value;
            const interval = this._result.interval;
            let label = valueFormatter(value);
            if (interval)
                label += ' \u00B1' + ((value - interval[0]) * 100 / value).toFixed(2) + '%';
            resultContent = barGraphGroup.addBar([value], [label], null, null, '#ccc', null);
        }
        return this._link ? ComponentBase.createLink(resultContent, this._label, this._link) : resultContent;
    }
}
