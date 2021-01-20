class FreshnessIndicator extends ComponentBase {
    constructor(lastDataPointDuration, testAgeTolerance, summary)
    {
        super('freshness-indicator');
        this._lastDataPointDuration = lastDataPointDuration;
        this._testAgeTolerance = testAgeTolerance;
        this._highlighted = false;

        this._renderIndicatorLazily = new LazilyEvaluatedFunction(this._renderIndicator.bind(this));
    }

    update(lastDataPointDuration, testAgeTolerance, highlighted)
    {
        this._lastDataPointDuration = lastDataPointDuration;
        this._testAgeTolerance = testAgeTolerance;
        this._highlighted = highlighted;
        this.enqueueToRender();
    }

    render()
    {
        super.render();
        this._renderIndicatorLazily.evaluate(this._lastDataPointDuration, this._testAgeTolerance, this._url, this._highlighted);

    }

    _renderIndicator(lastDataPointDuration, testAgeTolerance, url, highlighted)
    {
        const element = ComponentBase.createElement;
        if (!lastDataPointDuration) {
            this.renderReplace(this.content('container'), new SpinnerIcon);
            return;
        }

        const hoursSinceLastDataPoint = this._lastDataPointDuration / 3600 / 1000;
        const testAgeToleranceInHours = testAgeTolerance / 3600 / 1000;
        const rating = 1 / (1 + Math.exp(Math.log(1.2) * (hoursSinceLastDataPoint - testAgeToleranceInHours)));
        const hue = Math.round(120 * rating);
        const brightness = Math.round(30 + 50 * rating);
        const indicator = element('a', {id: 'cell', class: `${highlighted ? 'highlight' : ''}`});

        indicator.style.backgroundColor = `hsl(${hue}, 100%, ${brightness}%)`;
        this.renderReplace(this.content('container'), indicator);
    }

    static htmlTemplate()
    {
        return `<div id='container'></div>`;
    }

    static  cssTemplate()
    {
        return `
            div {
                margin: auto;
                height: 1.8rem;
                width: 1.8rem;
            }
            a {
                display: block;
                height:1.6rem;
                width:1.6rem;
                border: 0.1rem;
                border-color: white;
                border-style: solid;
                padding: 0;
            }

            a.highlight {
                height: 1.4rem;
                width: 1.4rem;
                border: 0.2rem;
                border-style: solid;
                border-color: #0099ff;
            }`;
    }
}


ComponentBase.defineElement('freshness-indicator', FreshnessIndicator);
