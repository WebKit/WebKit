
class AnalysisCategoryPage extends PageWithHeading {
    constructor()
    {
        super('Analysis', new AnalysisCategoryToolbar);
        this.toolbar().setFilterCallback(this.render.bind(this));
        this._renderedList = false;
        this._renderedFilter = false;
        this._fetched = false;
        this._errorMessage = null;
    }

    title()
    {
        var category = this.toolbar().currentCategory();
        return (category ? category.charAt(0).toUpperCase() + category.slice(1) + ' ' : '') + 'Analysis Tasks';
    }
    routeName() { return 'analysis'; }

    open(state)
    {
        var self = this;
        AnalysisTask.fetchAll().then(function () {
            self._fetched = true;
            self.render();
        }, function (error) {
            self._errorMessage = 'Failed to fetch the list of analysis tasks: ' + error;
            self.render();
        });
        super.open(state);
    }

    updateFromSerializedState(state, isOpen)
    {
        if (this.toolbar().setCategoryIfValid(state.category))
            this._renderedList = false;

        if (!isOpen)
            this.render();
    }

    render()
    {
        Instrumentation.startMeasuringTime('AnalysisCategoryPage', 'render');

        if (!this._renderedList) {
            super.render();
            this.toolbar().render();
        }

        if (this._errorMessage) {
            console.assert(!this._fetched);
            var element = ComponentBase.createElement;
            this.renderReplace(this.content().querySelector('tbody.analysis-tasks'),
                element('tr',
                    element('td', {colspan: 6}, this._errorMessage)));
            this._renderedList = true;
            return;
        }

        if (!this._fetched)
            return;

        if (!this._renderedList) {
            this._reconstructTaskList();
            this._renderedList = true;
        }

        var filter = this.toolbar().filter();
        if (filter || this._renderedFilter) {
            Instrumentation.startMeasuringTime('AnalysisCategoryPage', 'filterByKeywords');
            var keywordList = filter ? filter.toLowerCase().split(/\s+/) : [];
            var tableRows = this.content().querySelectorAll('tbody.analysis-tasks tr');
            for (var i = 0; i < tableRows.length; i++) {
                var row = tableRows[i];
                var textContent = row.textContent.toLowerCase();
                var display = null;
                for (var keyword of keywordList) {
                    if (textContent.indexOf(keyword) < 0) {
                        display = 'none';
                        break;
                    }
                }
                row.style.display = display;
            }
            this._renderedFilter = !!filter;
            Instrumentation.endMeasuringTime('AnalysisCategoryPage', 'filterByKeywords');
        }

        Instrumentation.endMeasuringTime('AnalysisCategoryPage', 'render');
    }

    _reconstructTaskList()
    {
        Instrumentation.startMeasuringTime('AnalysisCategoryPage', 'reconstructTaskList');

        console.assert(this.router());
        var currentCategory = this.toolbar().currentCategory();

        var tasks = AnalysisTask.all().filter(function (task) {
            return task.category() == currentCategory;
        }).sort(function (a, b) {
            if (a.hasPendingRequests() == b.hasPendingRequests())
                return b.createdAt() - a.createdAt();
            else if (a.hasPendingRequests()) // a < b
                return -1;
            else if (b.hasPendingRequests()) // a > b
                return 1;
            return 0;
        });

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;
        var router = this.router();
        this.renderReplace(this.content().querySelector('tbody.analysis-tasks'),
            tasks.map(function (task) {
                var status = AnalysisCategoryPage._computeStatus(task);
                return element('tr', [
                    element('td', {class: 'status'},
                        element('span', {class: status.class}, status.label)),
                    element('td', link(task.label(), router.url(`analysis/task/${task.id()}`))),    
                    element('td', {class: 'bugs'},
                        element('ul', task.bugs().map(function (bug) {
                            var url = bug.url();
                            var title = bug.title();
                            return element('li', url ? link(bug.label(), title, url, true) : title);
                        }))),
                    element('td', {class: 'author'}, task.author()),
                    element('td', {class: 'platform'}, task.platform().label()),
                    element('td', task.metric().fullName()),
                    ]);
            }));

        Instrumentation.endMeasuringTime('AnalysisCategoryPage', 'reconstructTaskList');
    }

    static _computeStatus(task)
    {
        if (task.hasPendingRequests())
            return {label: task.requestLabel(), class: 'bisecting'};

        var type = task.changeType();
        switch (type) {
        case 'regression':
            return {label: 'Regression', class: type};
        case 'progression':
            return {label: 'Progression', class: type};
        case 'unchanged':
            return {label: 'No change', class: type};
        case 'inconclusive':
            return {label: 'Inconclusive', class: type};
        }

        if (task.hasResults())
            return {label: 'New results', class: 'bisecting'};

        return {label: 'Unconfirmed', class: 'unconfirmed'};
    }


    static htmlTemplate()
    {
        return `
            <div class="analysis-task-category">
                <table>
                    <thead>
                        <tr>
                            <td class="status">Status</td>
                            <td>Name</td>
                            <td>Bugs</td>
                            <td>Author</td>
                            <td>Platform</td>
                            <td>Test Metric</td>
                        </tr>
                    </thead>
                    <tbody class="analysis-tasks"></tbody>
                </table>
            </div>`;
    }

    static cssTemplate()
    {
        return `
            .analysis-task-category {
                width: calc(100% - 2rem);
                margin: 1rem;
            }

            .analysis-task-category table {
                width: 100%;
                border: 0;
                border-collapse: collapse;
            }

            .analysis-task-category td,
            .analysis-task-category th {
                border: none;
                border-collapse: collapse;
                text-align: left;
                font-size: 0.9rem;
                font-weight: normal;
            }

            .analysis-task-category thead td {
                color: #f96;
                font-weight: inherit;
                font-size: 1.1rem;
                padding: 0.2rem 0.4rem;
            }

            .analysis-task-category tbody td {
                border-top: solid 1px #eee;
                border-bottom: solid 1px #eee;
                padding: 0.2rem 0.2rem;
            }

            .analysis-task-category .bugs ul,
            .analysis-task-category .bugs li {
                padding: 0;
                margin: 0;
                list-style: none;
            }

            .analysis-task-category .status,
            .analysis-task-category .author,
            .analysis-task-category .platform {
                text-align: center;
            }

            .analysis-task-category .status span {
                display: inline;
                white-space: nowrap;
                border-radius: 0.3rem;
                padding: 0.2rem 0.3rem;
                font-size: 0.8rem;
            }

            .analysis-task-category .status .regression {
                background: #c33;
                color: #fff;
            }

            .analysis-task-category .status .progression {
                background: #36f;
                color: #fff;
            }

            .analysis-task-category .status .unchanged {
                background: #ccc;
                color: white;
            }

            .analysis-task-category .status .inconclusive {
                background: #666;
                color: white;
            }

            .analysis-task-category .status .bisecting {
                background: #ff9;
            }

            .analysis-task-category .status .unconfirmed {
                background: #f96;
            }
`;
    }
}
