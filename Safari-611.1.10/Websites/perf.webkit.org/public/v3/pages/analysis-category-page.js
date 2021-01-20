
class AnalysisCategoryPage extends PageWithHeading {
    constructor()
    {
        super('Analysis');
        this._categoryToolbar = this.content().querySelector('analysis-category-toolbar').component();
        this._categoryToolbar.setCategoryPage(this);
        this._renderedList = false;
        this._renderedFilter = false;
        this._fetched = false;
        this._errorMessage = null;
    }

    title()
    {
        var category = this._categoryToolbar.currentCategory();
        return (category ? category.charAt(0).toUpperCase() + category.slice(1) + ' ' : '') + 'Analysis Tasks';
    }
    routeName() { return 'analysis'; }

    open(state)
    {
        var self = this;
        AnalysisTask.fetchAll().then(function () {
            self._fetched = true;
            self.enqueueToRender();
        }, function (error) {
            self._errorMessage = 'Failed to fetch the list of analysis tasks: ' + error;
            self.enqueueToRender();
        });
        super.open(state);
    }

    serializeState()
    {
        return this.stateForCategory(this._categoryToolbar.currentCategory());
    }

    stateForCategory(category)
    {
        var state = {category: category};
        var filter = this._categoryToolbar.filter();
        if (filter)
            state.filter = filter;
        return state;
    }

    updateFromSerializedState(state, isOpen)
    {
        if (state.category instanceof Set)
            state.category = Array.from(state.category.values())[0];
        if (state.filter instanceof Set)
            state.filter = Array.from(state.filter.values())[0];

        if (this._categoryToolbar.setCategoryIfValid(state.category))
            this._renderedList = false;

        if (state.filter)
            this._categoryToolbar.setFilter(state.filter);

        if (!isOpen)
            this.enqueueToRender();
    }

    filterDidChange(shouldUpdateState)
    {
        this.enqueueToRender();
        if (shouldUpdateState)
            this.scheduleUrlStateUpdate();
    }

    render()
    {
        Instrumentation.startMeasuringTime('AnalysisCategoryPage', 'render');

        super.render();
        this._categoryToolbar.enqueueToRender();

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

        var filter = this._categoryToolbar.filter();
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
        const currentCategory = this._categoryToolbar.currentCategory();

        const tasks = AnalysisTask.all().filter((task) => task.category() == currentCategory).sort((a, b) => {
            if (a.hasPendingRequests() == b.hasPendingRequests())
                return b.createdAt() - a.createdAt();
            else if (a.hasPendingRequests()) // a < b
                return -1;
            else if (b.hasPendingRequests()) // a > b
                return 1;
            return 0;
        });

        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        const router = this.router();
        this.renderReplace(this.content().querySelector('tbody.analysis-tasks'),
            tasks.map((task) => {
                const status = AnalysisCategoryPage._computeStatus(task);
                return element('tr', [
                    element('td', {class: 'status'},
                        element('span', {class: status.class}, status.label)),
                    element('td', link(task.label(), router.url(`analysis/task/${task.id()}`))),    
                    element('td', {class: 'bugs'},
                        element('ul', task.bugs().map((bug) => {
                            const url = bug.url();
                            const title = bug.title();
                            return element('li', url ? link(bug.label(), title, url, true) : title);
                        }))),
                    element('td', {class: 'author'}, task.author()),
                    element('td', {class: 'platform'}, task.platform() ? task.platform().label() : null),
                    element('td', task.metric() ? task.metric().fullName() : null),
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
            <div class="toolbar-container"><analysis-category-toolbar></analysis-category-toolbar></div>
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
            .toolbar-container {
                text-align: center;
            }

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
