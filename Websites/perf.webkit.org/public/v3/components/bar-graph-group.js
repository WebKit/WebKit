
class BarGraphGroup {
    constructor(formatter)
    {
        this._bars = [];
        this._formatter = formatter;
    }

    addBar(value, interval)
    {
        var newBar = new SingleBarGraph(this);
        this._bars.push({bar: newBar, value: value, interval: interval});
        return newBar;
    }

    render()
    {
        Instrumentation.startMeasuringTime('BarGraphGroup', 'render');

        var min = Infinity;
        var max = -Infinity;
        for (var entry of this._bars) {
            min = Math.min(min, entry.interval ? entry.interval[0] : entry.value);
            max = Math.max(max, entry.interval ? entry.interval[1] : entry.value);
        }

        for (var entry of this._bars) {
            var value = entry.value;
            var formattedValue = this._formatter(value);
            if (entry.interval)
                formattedValue += ' \u00B1' + ((value - entry.interval[0]) * 100 / value).toFixed(2) + '%';

            var diff = (max - min);
            var range = diff * 1.2;
            var start = min - (range - diff) / 2;

            entry.bar.update((value - start) / range, formattedValue);
            entry.bar.render();
        }

        Instrumentation.endMeasuringTime('BarGraphGroup', 'render');
    }
}

class SingleBarGraph extends ComponentBase {

    constructor(group)
    {
        console.assert(group instanceof BarGraphGroup);
        super('single-bar-graph');
        this._percentage = 0;
        this._label = null;
    }

    update(percentage, label)
    {
        this._percentage = percentage;
        this._label = label;
    }

    render()
    {
        this.content().querySelector('.percentage').style.width = `calc(${this._percentage * 100}% - 2px)`;
        this.content().querySelector('.label').textContent = this._label;
    }

    static htmlTemplate()
    {
        return `<div class="single-bar-graph"><div class="percentage"></div><div class="label">-</div></div>`;
    }

    static cssTemplate()
    {
        return `
            .single-bar-graph {
                position: relative;
                display: block;
                background: #eee;
                height: 100%;
                overflow: hidden;
                text-decoration: none;
                color: black;
            }
            .single-bar-graph .percentage {
                position: absolute;
                top: 1px;
                left: 1px;
                background: #ccc;
                height: calc(100% - 2px);
            }
            .single-bar-graph .label {
                position: absolute;
                top: calc(50% - 0.35rem);
                left: 0;
                width: 100%;
                height: 100%;
                font-size: 0.8rem;
                line-height: 0.8rem;
                text-align: center;
            }
        `;
    }

}
