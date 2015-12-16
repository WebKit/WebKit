
class AnalysisTaskPage extends PageWithHeading {
    constructor()
    {
        super('Analysis Task');
        this._taskId = null;
        this._task = null;
        this._errorMessage = null;
    }

    title() { return this._task ? this._task.label() : 'Analysis Task'; }
    routeName() { return 'analysis/task'; }

    updateFromSerializedState(state)
    {
        var taskId = parseInt(state.remainingRoute);
        if (taskId != state.remainingRoute) {
            this._errorMessage = `Invalid analysis task ID: ${state.remainingRoute}`;
            return;
        }

        var self = this;
        AnalysisTask.fetchById(taskId).then(function (task) {
            self._task = task;
            self.render();
        });
    }

    render()
    {
        super.render();

        Instrumentation.startMeasuringTime('AnalysisTaskPage', 'render');

        this.content().querySelector('.error-message').textContent = this._errorMessage || '';

        var v2URL = location.href.replace('/v3/', '/v2/');
        this.content().querySelector('.overview-chart').innerHTML = `Not ready. Use <a href="${v2URL}">v2 page</a> for now.`;

        if (this._task) {
            this.renderReplace(this.content().querySelector('.analysis-task-name'), this._task.name());
        }

        Instrumentation.endMeasuringTime('AnalysisTaskPage', 'render');
    }

    static htmlTemplate()
    {
        return `
            <h2 class="analysis-task-name"></h2>
            <p class="error-message"></p>
            <div class="overview-chart"></div>
`;
    }

    static cssTemplate()
    {
        return `
            .analysis-task-name {
                font-size: 1.2rem;
                font-weight: inherit;
                color: #c93;
                margin: 0 1rem;
                padding: 0;
            }

            .error-message:not(:empty) {
                margin: 1rem;
                padding: 0;
            }

            .overview-chart {
                width: auto;
                height: 10rem;
                margin: 1rem;
                border: solid 0px red;
            }
`;
    }
}
