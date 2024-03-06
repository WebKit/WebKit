
class DashboardChartStatusView extends ComponentBase {

    constructor(metric, chart)
    {
        super('chart-status-view');
        this._statusEvaluator = new ChartStatusEvaluator(chart, metric);
        this._renderLazily = new LazilyEvaluatedFunction((status) => {
            status = status || {};
            this.content('current-value').textContent = status.currentValue || '';
            this.content('comparison').textContent = status.label || '';
            this.content('comparison').className = status.comparison || '';
        });
    }

    render()
    {
        this._renderLazily.evaluate(this._statusEvaluator.status());
    }

    static htmlTemplate()
    {
        return `<span id="current-value"></span> <span id="comparison"></span>`;
    }

    static cssTemplate()
    {
        return `
            :host {
                display: block;
            }

            #comparison {
                padding-left: 0.5rem;
            }

            #comparison.worse {
                color: #c33;
            }

            #comparison.better {
                color: #33c;
            }`;
    }
}
