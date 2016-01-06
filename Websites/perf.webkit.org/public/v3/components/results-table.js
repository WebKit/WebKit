class ResultsTable extends ComponentBase {
    constructor(name)
    {
        super(name);
        this._repositoryList = [];
        this._valueFormatter = null;
    }

    setValueFormatter(valueFormatter) { this._valueFormatter = valueFormatter; }

    render()
    {
        if (!this._valueFormatter)
            return;

        Instrumentation.startMeasuringTime('ResultsTable', 'render');

        var rowGroups = this.buildRowGroups();

        var extraRepositories = [];
        var repositoryList = this._computeRepositoryList(rowGroups, extraRepositories);

        var barGraphGroup = new BarGraphGroup(this._valueFormatter);
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var tableBodies = rowGroups.map(function (group) {
            var groupHeading = group.heading;
            var revisionSupressionCount = {};

            return element('tbody', group.rows.map(function (row, rowIndex) {
                var cells = row.buildHeading(barGraphGroup);

                if (groupHeading && !rowIndex)
                    cells.unshift(element('th', {rowspan: group.rows.length}, groupHeading));

                var rootSet = row.rootSet();
                repositoryList.forEach(function (repository) {
                    var commit = rootSet ? rootSet.commitForRepository(repository) : null;

                    if (revisionSupressionCount[repository.id()]) {
                        revisionSupressionCount[repository.id()]--;
                        return;
                    }

                    var succeedingRowIndex = rowIndex + 1;
                    while (succeedingRowIndex < group.rows.length) {
                        var succeedingRootSet = group.rows[succeedingRowIndex].rootSet();
                        if (succeedingRootSet && commit != succeedingRootSet.commitForRepository(repository))
                            break;
                        succeedingRowIndex++;
                    }
                    var rowSpan = succeedingRowIndex - rowIndex;
                    var attributes = {class: 'revision'};
                    if (rowSpan > 1) {
                        revisionSupressionCount[repository.id()] = rowSpan - 1;
                        attributes['rowspan'] = rowSpan;                       
                    }
                    if (rowIndex + rowSpan >= group.rows.length)
                        attributes['class'] += ' lastRevision';

                    var content = 'Missing';
                    if (commit) {
                        var url = commit.url();
                        content = url ? link(commit.label(), url) : commit.label();
                    }

                    cells.push(element('td', attributes, content));
                });

                return element('tr', [cells, row.additionalColumns()]);
            }));
        });

        this.renderReplace(this.content().querySelector('table'), [
            element('thead', [
                this.heading(),
                element('th', 'Result'),
                repositoryList.map(function (repository) { return element('th', repository.label()); }),
                this.additionalHeading(),
            ]),
            tableBodies,
        ]);

        this.renderReplace(this.content().querySelector('.results-table-extra-repositories'),
            extraRepositories.map(function (commit) { return element('li', commit.title()); }));

        barGraphGroup.render();

        Instrumentation.endMeasuringTime('ResultsTable', 'render');
    }

    heading() { throw 'NotImplemented'; }
    additionalHeading() { return []; }
    buildRowGroups() { throw 'NotImplemented'; }

    _computeRepositoryList(rowGroups, extraRepositories)
    {
        var allRepositories = Repository.all().sort(function (a, b) {
            if (a.hasUrlForRevision() == b.hasUrlForRevision()) {
                if (a.name() > b.name())
                    return 1;
                if (a.name() < b.name())
                    return -1;
                return 0;
            }
            return a.hasUrlForRevision() ? -1 /* a < b */ : 1; // a > b
        });
        var rootSets = [];
        for (var group of rowGroups) {
            for (var row of group.rows) {
                var rootSet = row.rootSet();
                if (rootSet)
                    rootSets.push(rootSet);
            }
        }
        if (!rootSets.length)
            return [];

        var repositoryPresenceMap = {};
        for (var repository of allRepositories) {
            var someCommit = rootSets[0].commitForRepository(repository);
            if (RootSet.containsMultipleCommitsForRepository(rootSets, repository))
                repositoryPresenceMap[repository.id()] = true;
            else if (someCommit)
                extraRepositories.push(someCommit);
        }
        return allRepositories.filter(function (repository) { return repositoryPresenceMap[repository.id()]; });
    }

    static htmlTemplate()
    {
        return `<table class="results-table"></table><ul class="results-table-extra-repositories"></ul>`;
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

            .results-table single-bar-graph {
                display: block;
                width: 7rem;
                height: 1.2rem;
            }

            .results-table-extra-repositories {
                list-style: none;
                margin: 0;
                padding: 0.5rem 0 0 0.5rem;
                font-size: 0.8rem;
            }

            .results-table-extra-repositories li {
                display: inline;
            }

            .results-table-extra-repositories li:not(:last-child):after {
                content: ', ';
            }
        `;
    }
}

class ResultsTableRow {
    constructor(heading, rootSet)
    {
        this._heading = heading;
        this._result = null;
        this._link = null;
        this._label = '-';
        this._rootSet = rootSet;
        this._additionalColumns = [];
    }

    rootSet() { return this._rootSet; }

    setResult(result) { this._result = result; }
    setLink(link, label)
    {
        this._link = link;
        this._label = label;
    }

    additionalColumns() { return this._additionalColumns; }
    setAdditionalColumns(additionalColumns) { this._additionalColumns = additionalColumns; }

    buildHeading(barGraphGroup)
    {
        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        var resultContent = this._result ? barGraphGroup.addBar(this._result.value, this._result.interval) : this._label;
        return [
            element('th', this._heading),
            element('td', this._link ? link(resultContent, this._label, this._link) : resultContent),
        ];
    }
}
