class FreshnessIndicator extends ComponentBase {
    constructor(lastDataPointDuration, testAgeTolerance, summary, url)
    {
        super('freshness-indicator');
        this._lastDataPointDuration = lastDataPointDuration;
        this._summary = summary;
        this._testAgeTolerance = testAgeTolerance;
        this._url = url;

        this._renderIndicatorLazily = new LazilyEvaluatedFunction(this._renderIndicator.bind(this));
    }

    update(lastDataPointDuration, testAgeTolerance, summary, url)
    {
        this._lastDataPointDuration = lastDataPointDuration;
        this._summary = summary;
        this._testAgeTolerance = testAgeTolerance;
        this._url = url;
        this.enqueueToRender();
    }

    render()
    {
        super.render();
        this._renderIndicatorLazily.evaluate(this._lastDataPointDuration, this._testAgeTolerance, this._summary, this._url);

    }

    _renderIndicator(lastDataPointDuration, testAgeTolerance, summary, url)
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
        const indicator = element('a', {id: 'cell', title: summary, href: url});

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
                height: 1.8rem;
                width: 1.8rem;
                padding-top: 0.1rem;
            }
            a {
                display: block;
                height:1.6rem;
                width:1.6rem;
                margin: 0.1rem;
                padding: 0;
            }

            a:hover {
                height: 1.8rem;
                width: 1.8rem;
                margin: 0rem;
                padding: 0;
            }`;
    }
}


ComponentBase.defineElement('freshness-indicator', FreshnessIndicator);