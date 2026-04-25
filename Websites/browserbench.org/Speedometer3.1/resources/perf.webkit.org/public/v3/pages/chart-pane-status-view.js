
class ChartPaneStatusView extends ComponentBase {
    constructor(metric, chart)
    {
        super('chart-pane-status-view');

        this._chart = chart;
        this._status = new ChartStatusEvaluator(chart, metric);
        this._revisionRange = new ChartRevisionRange(chart);
        this._currentRepository = null;

        this._renderStatusLazily = new LazilyEvaluatedFunction(this._renderStatus.bind(this));
        this._renderBuildRevisionTableLazily = new LazilyEvaluatedFunction(this._renderBuildRevisionTable.bind(this));
    }

    render()
    {
        super.render();

        this._renderStatusLazily.evaluate(this._status.status());

        const indicator = this._chart.currentIndicator();
        const build = indicator ? indicator.point.build() : null;
        this._renderBuildRevisionTableLazily.evaluate(build, this._currentRepository, this._revisionRange.revisionList());
    }

    _renderStatus(status)
    {
        status = status || {};
        let currentValue = status.currentValue || '';
        if (currentValue)
            currentValue += ` (${status.valueDelta} / ${status.relativeDelta})`;

        this.content('current-value').textContent = currentValue;
        this.content('comparison').textContent = status.label || '';
        this.content('comparison').className = status.comparison || '';
    }

    _renderBuildRevisionTable(build, currentRepository, revisionList)
    {
        const element = ComponentBase.createElement;
        const link = ComponentBase.createLink;
        let tableContent = [];

        if (build) {
            const url = build.url();
            const buildTag = build.buildTag();
            tableContent.push(element('tr', [
                element('td', 'Build'),
                element('td', {colspan: 2}, [
                    url ? link(buildTag, build.label(), url, true) : buildTag,
                    ` (${Metric.formatTime(build.buildTime())})`
                ]),
            ]));
        }

        if (revisionList) {
            for (let info of revisionList) {
                const selected = info.repository == this._currentRepository;
                const action = () => {
                    this.dispatchAction('openRepository', this._currentRepository == info.repository ? null : info.repository);
                };

                tableContent.push(element('tr', {class: selected ? 'selected' : ''}, [
                    element('td', info.repository.name()),
                    element('td', info.url ? link(info.label, info.label, info.url, true) : info.label),
                    element('td', {class: 'commit-viewer-opener'}, link('\u00BB', action)),
                ]));
            }
        }

        this.renderReplace(this.content('build-revision'), tableContent);
    }

    setCurrentRepository(repository)
    {
        this._currentRepository = repository;
        this.enqueueToRender();
    }

    static htmlTemplate()
    {
        return `
            <div id="chart-pane-status">
                <h3 id="current-value"></h3>
                <span id="comparison"></span>
            </div>
            <table id="build-revision"></table>
        `;
    }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + `
            #chart-pane-status {
                display: block;
                text-align: center;
            }

            #current-value,
            #comparison {
                display: block;
                margin: 0;
                padding: 0;
                font-weight: normal;
                font-size: 1rem;
            }

            #comparison.worse {
                color: #c33;
            }

            #comparison.better {
                color: #33c;
            }

            #build-revision {
                line-height: 1rem;
                font-size: 0.9rem;
                font-weight: normal;
                padding: 0;
                margin: 0;
                margin-top: 0.5rem;
                border-bottom: solid 1px #ccc;
                border-collapse: collapse;
                width: 100%;
            }

            #build-revision th,
            #build-revision td {
                font-weight: inherit;
                border-top: solid 1px #ccc;
                padding: 0.2rem 0.2rem;
            }
            
            #build-revision .selected > th,
            #build-revision .selected > td {
                background: rgba(204, 153, 51, 0.1);
            }

            #build-revision .commit-viewer-opener {
                width: 1rem;
            }

            #build-revision .commit-viewer-opener a {
                text-decoration: none;
                color: inherit;
                font-weight: inherit;
            }
        `;
    }
}
