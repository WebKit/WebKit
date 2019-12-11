
class BarGraphGroup {
    constructor()
    {
        this._bars = [];
        this._computeRangeLazily = new LazilyEvaluatedFunction(this._computeRange.bind(this));
        this._showLabels = false;
    }

    addBar(values, valueLabels, mean, interval, barColor, meanIndicatorColor)
    {
        const bar = new BarGraph(this, values, valueLabels, mean, interval, barColor, meanIndicatorColor);
        this._bars.push({bar, values, interval});
        return bar;
    }

    showLabels() { return this._showLabels; }
    setShowLabels(showLabels)
    {
        this._showLabels = showLabels;
        for (const entry of this._bars)
            entry.bar.enqueueToRender();
    }

    range()
    {
        return this._computeRangeLazily.evaluate(...this._bars);
    }

    _computeRange(...bars)
    {
        Instrumentation.startMeasuringTime('BarGraphGroup', 'updateGroup');

        let min = Infinity;
        let max = -Infinity;
        for (const entry of bars) {
            for (const value of entry.values) {
                if (isNaN(value))
                    continue;
                min = Math.min(min, value);
                max = Math.max(max, value);
            }
            if (entry.interval) {
                for (const value of entry.interval) {
                    if (isNaN(value))
                        continue;
                    min = Math.min(min, value);
                    max = Math.max(max, value);
                }
            }
        }

        const diff = max - min;
        min -= diff * 0.1;
        max += diff * 0.1;

        const xForValue = (value, width) => (value - min) / (max - min) * width;
        const barRangeForValue = (value, width) => [0, (value - min) / (max - min) * width];

        Instrumentation.endMeasuringTime('BarGraphGroup', 'updateGroup');

        return {min, max, xForValue, barRangeForValue};
    }
}

class BarGraph extends ComponentBase {
    constructor(group, values, valueLabels, mean, interval, barColor, meanIndicatorColor)
    {
        super('bar-graph');
        this._group = group;
        this._values = values;
        this._valueLabels = valueLabels;
        this._mean = mean;
        this._interval = interval;
        this._barColor = barColor;
        this._meanIndicatorColor = meanIndicatorColor;
        this._resizeCanvasLazily = new LazilyEvaluatedFunction(this._resizeCanvas.bind(this));
        this._renderCanvasLazily = new LazilyEvaluatedFunction(this._renderCanvas.bind(this));
        this._renderLabelsLazily = new LazilyEvaluatedFunction(this._renderLabels.bind(this));
    }

    render()
    {
        Instrumentation.startMeasuringTime('SingleBarGraph', 'render');

        if (!this._values)
            return false;

        const range = this._group.range();
        const showLabels = this._group.showLabels();

        const canvas = this.content('graph');
        const element = this.element();
        const width = element.offsetWidth;
        const height = element.offsetHeight;
        const scale = this._resizeCanvasLazily.evaluate(canvas, width, height);

        const step = this._renderCanvasLazily.evaluate(canvas, width, height, scale, this._values,
            showLabels ? null : this._mean, showLabels ? null : this._interval, range);

        this._renderLabelsLazily.evaluate(canvas, step, showLabels ? this._valueLabels : null);

        Instrumentation.endMeasuringTime('SingleBarGraph', 'render');
    }

    _resizeCanvas(canvas, width, height)
    {
        const scale = window.devicePixelRatio;
        canvas.width = width * scale;
        canvas.height = height * scale;
        canvas.style.width = width + 'px';
        canvas.style.height = height + 'px';
        return scale;
    }

    _renderCanvas(canvas, width, height, scale, values, mean, interval, range)
    {
        const context = canvas.getContext('2d');
        context.scale(scale, scale);
        context.clearRect(0, 0, width, height);

        context.fillStyle = this._barColor;
        context.strokeStyle = this._meanIndicatorColor;
        context.lineWidth = 1;

        const step = Math.floor(height / values.length);
        for (let i = 0; i < values.length; i++) {
            const value = values[i];
            if (isNaN(value))
                continue;
            const barWidth = range.xForValue(value, width);
            const barRange = range.barRangeForValue(value, width);
            const y = i * step;
            context.fillRect(0, y, barWidth, step - 1);
        }

        const filteredValues = values.filter((value) => !isNaN(value));
        if (mean) {
            const x = range.xForValue(mean, width);
            context.beginPath();
            context.moveTo(x, 0);
            context.lineTo(x, height);
            context.stroke();
        }

        if (interval) {
            const x1 = range.xForValue(interval[0], width);
            const x2 = range.xForValue(interval[1], width);

            const errorBarHeight = 10;
            const endBarY1 = height / 2 - errorBarHeight / 2;
            const endBarY2 = height / 2 + errorBarHeight / 2;

            context.beginPath();
            context.moveTo(x1, endBarY1);
            context.lineTo(x1, endBarY2);
            context.moveTo(x1, height / 2);
            context.lineTo(x2, height / 2);
            context.moveTo(x2, endBarY1);
            context.lineTo(x2, endBarY2);
            context.stroke();
        }

        return step;
    }

    _renderLabels(canvas, step, valueLabels)
    {
        if (!valueLabels)
            valueLabels = [];

        const element = ComponentBase.createElement;
        this.renderReplace(this.content('labels'), valueLabels.map((label, i) => {
            const labelElement = element('div', {class: 'label'}, label);
            labelElement.style.top = (i + 0.5) * step + 'px';
            return labelElement;
        }));
        canvas.style.opacity = valueLabels.length ? 0.5 : 1;
    }

    static htmlTemplate()
    {
        return `<canvas id="graph"></canvas><div id="labels"></div>`;
    }

    static cssTemplate()
    {
        return `
            :host {
                display: block !important;
                overflow: hidden;
                position: relative;
            }
            .label {
                position: absolute;
                left: 0;
                width: 100%;
                text-align: center;
                font-size: 0.8rem;
                line-height: 0.8rem;
                margin-top: -0.4rem;
            }
        `;
    }
}

ComponentBase.defineElement('bar-graph', BarGraph);
