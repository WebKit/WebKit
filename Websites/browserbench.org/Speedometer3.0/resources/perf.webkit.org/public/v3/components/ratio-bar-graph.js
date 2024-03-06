class RatioBarGraph extends ComponentBase {

    constructor()
    {
        super('ratio-bar-graph');
        this._ratio = null;
        this._label = null;
        this._shouldRender = true;
        this._ratioBarGraph = this.content().querySelector('.ratio-bar-graph');
    }

    update(ratio, label, showWarningIcon)
    {
        console.assert(typeof(ratio) == 'number');
        this._ratio = ratio;
        this._label = label;
        this._showWarningIcon = showWarningIcon;
        this._shouldRender = true;
    }

    render()
    {
        if (!this._shouldRender)
            return;
        this._shouldRender = false;
        var element = ComponentBase.createElement;

        var children = [element('div', {class: 'separator'})];
        if (this._showWarningIcon) {
            if (this._ratio && this._ratio < -0.4)
                this._ratioBarGraph.classList.add('warning-on-right');
            else
                this._ratioBarGraph.classList.remove('warning-on-right');
            children.push(new WarningIcon);
        }

        var barClassName = 'bar';
        var labelClassName = 'label';
        if (this._ratio) {
            var ratioType = this._ratio > 0 ? 'better' : 'worse';
            barClassName = [barClassName, ratioType].join(' ');
            labelClassName = [labelClassName, ratioType].join(' ');
            children.push(element('div', {class: barClassName, style: 'width:' + Math.min(Math.abs(this._ratio * 100), 50) + '%'}));
        }
        if (this._label)
            children.push(element('div', {class: labelClassName}, this._label));

        this.renderReplace(this._ratioBarGraph, children);
    }

    static htmlTemplate()
    {
        return `<div class="ratio-bar-graph"></div>`;
    }

    static cssTemplate()
    {
        return `
            .ratio-bar-graph {
                position: relative;
                display: block;
                margin-left: auto;
                margin-right: auto;
                width: 10rem;
                height: 2.5rem;
                overflow: hidden;
                text-decoration: none;
                color: black;
            }
            .ratio-bar-graph warning-icon {
                position: absolute;
                display: block;
                top: 0;
            }
            .ratio-bar-graph:not(.warning-on-right) warning-icon {
                left: 0;
            }
            .ratio-bar-graph.warning-on-right warning-icon {
                transform: scaleX(-1);
                right: 0;
            }
            .ratio-bar-graph .separator {
                position: absolute;
                left: 50%;
                width: 0px;
                height: 100%;
                border-left: solid 1px #ccc;
            }
            .ratio-bar-graph .bar {
                position: absolute;
                left: 50%;
                top: 0.5rem;
                height: calc(100% - 1rem);
                background: #ccc;
            }
            .ratio-bar-graph .bar.worse {
                transform: translateX(-100%);
                background: #c33;
            }
            .ratio-bar-graph .bar.better {
                background: #3c3;
            }
            .ratio-bar-graph .label {
                position: absolute;
                line-height: 2.5rem;
            }
            .ratio-bar-graph .label.worse {
                text-align: left;
                left: calc(50% + 0.2rem);
            }
            .ratio-bar-graph .label.better {
                text-align: right;
                right: calc(50% + 0.2rem);
            }
        `;
    }

}