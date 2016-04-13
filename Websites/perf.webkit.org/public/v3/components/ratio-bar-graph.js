class RatioBarGraph extends ComponentBase {

    constructor()
    {
        super('ratio-bar-graph');
        this._ratio = 0;
        this._label = null;
        this._shouldRender = true;
        this._bar = this.content().querySelector('.bar');
        this._labelContainer = this.content().querySelector('.label');
    }

    update(ratio, label)
    {
        console.assert(ratio >= -1 && ratio <= 1);
        this._ratio = ratio;
        this._label = label;
        this._shouldRender = true;
    }

    render()
    {
        if (!this._shouldRender)
            return;

        var percent = Math.abs(this._ratio * 100);
        this._labelContainer.textContent = this._label;
        this._bar.style.width = Math.min(percent, 50) + '%';
        this._bar.parentNode.className = 'ratio-bar-graph ' + (this._ratio > 0 ? 'better' : 'worse');

        this._shouldRender = false;
    }

    static htmlTemplate()
    {
        return `<div class="ratio-bar-graph"><div class="seperator"></div><div class="bar"></div><div class="label"></div></div>`;
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
            .ratio-bar-graph .seperator {
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
            .ratio-bar-graph.worse .bar {
                transform: translateX(-100%);
                background: #c33;
            }
            .ratio-bar-graph.better .bar {
                background: #3c3;
            }
            .ratio-bar-graph .label {
                position: absolute;
                line-height: 2.5rem;
            }
            .ratio-bar-graph.worse .label {
                text-align: left;
                left: calc(50% + 0.2rem);
            }
            .ratio-bar-graph.better .label {
                text-align: right;
                right: calc(50% + 0.2rem);
            }
        `;
    }

}