
class ChartsToolbar extends DomainControlToolbar {
    constructor()
    {
        super('chars-toolbar', 7);

        this._numberOfDaysCallback = null;
        this._inputElement = this.content().querySelector('input');
        this._labelSpan = this.content().querySelector('.day-count');

        this._inputElement.addEventListener('change', this._inputValueMayHaveChanged.bind(this));
        this._inputElement.addEventListener('mousemove', this._inputValueMayHaveChanged.bind(this));

        this._addPaneCallback = null;
        this._paneSelector = this.content().querySelector('pane-selector').component();
        this._paneSelectorOpener = this.content().querySelector('.pane-selector-opener');
        this._paneSelectorContainer = this.content().querySelector('.pane-selector-container');

        this._paneSelector.setCallback(this._addPane.bind(this));
        this._paneSelectorOpener.addEventListener('click', this._togglePaneSelector.bind(this));
        
        var self = this;
        this._paneSelectorOpener.addEventListener('mouseenter', function () {
            self._openPaneSelector(false);
            temporarilyIgnoreMouseleave = true;
            setTimeout(function () { temporarilyIgnoreMouseleave = false; }, 0);
        });
        this._paneSelectorContainer.style.display = 'none';

        var temporarilyIgnoreMouseleave = false;
        this._paneSelectorContainer.addEventListener('mousemove', function () {
            temporarilyIgnoreMouseleave = true; // Workaround webkit.org/b/152170
            setTimeout(function () { temporarilyIgnoreMouseleave = false; }, 0);
        });
        this._paneSelectorContainer.addEventListener('mouseleave', function (event) {
            setTimeout(function () {
                if (!temporarilyIgnoreMouseleave)
                    self._closePaneSelector();
            }, 0);
        });
    }

    render()
    {
        super.render();
        this._paneSelector.render();
        this._labelSpan.textContent = this._numberOfDays;
        this._inputElement.value = this._numberOfDays;
    }

    setNumberOfDaysCallback(callback)
    {
        console.assert(!callback || callback instanceof Function);
        this._numberOfDaysCallback = callback;
    }

    setAddPaneCallback(callback)
    {
        console.assert(!callback || callback instanceof Function);
        this._addPaneCallback = callback;
    }

    setStartTime(startTime)
    {
        if (startTime)
            super.setStartTime(startTime);
        else
            super.setNumberOfDays(7);
    }

    _inputValueMayHaveChanged(event)
    {
        var numberOfDays = parseInt(this._inputElement.value);
        if (this.numberOfDays() != numberOfDays && this._numberOfDaysCallback)
            this._numberOfDaysCallback(numberOfDays, event.type == 'change');
    }


    _togglePaneSelector(event)
    {
        event.preventDefault();
        if (this._paneSelectorContainer.style.display == 'none')
            this._openPaneSelector(true);
        else
            this._closePaneSelector();
    }

    _openPaneSelector(shouldFocus)
    {
        var opener = this._paneSelectorOpener;
        var container = this._paneSelectorContainer;
        opener.parentNode.className = 'selected';
        var right = container.parentNode.offsetWidth - (opener.offsetLeft + opener.offsetWidth);

        container.style.display = 'block';
        container.style.right = right + 'px';

        if (shouldFocus)
            this._paneSelector.focus();
    }

    _closePaneSelector()
    {
        this._paneSelectorOpener.parentNode.className = '';
        this._paneSelectorContainer.style.display = 'none';
    }

    _addPane(platform, metric)
    {
        if (!this._addPaneCallback)
            return;

        this._closePaneSelector();
        this._addPaneCallback(platform, metric);
    }


    static htmlTemplate()
    {
        return `
            <nav class="charts-toolbar">
                <ul class="buttoned-toolbar">
                    <li><a href="#" class="pane-selector-opener">Add pane</a></li>
                </ul>
                <ul class="buttoned-toolbar">
                    <li class="start-time-slider"><label><input type="range" min="7" max="365" step="1"><span class="day-count">?</span> days</label></li>
                </ul>
                <div class="pane-selector-container">
                    <pane-selector></pane-selector>
                </div>
            </nav>`;
    }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + `

            .buttoned-toolbar li a.pane-selector-opener:hover {
                background: rgba(204, 153, 51, 0.1);
            }

            .charts-toolbar > .pane-selector-container {
                position: absolute;
                right: 1rem;
                margin: 0;
                margin-top: -0.2rem;
                margin-right: -0.5rem;
                padding: 1rem;
                border: solid 1px #ccc;
                border-radius: 0.2rem;
                background: rgba(255, 255, 255, 0.8);
                -webkit-backdrop-filter: blur(0.5rem);
            }

            .start-time-slider {
                line-height: 1em;
                font-size: 0.9rem;
            }

            .start-time-slider label {
                display: inline-block;
            }

            .start-time-slider input {
                height: 0.8rem;
            }

            .start-time-slider .numberOfDays {
                display: inline-block;
                text-align: right;
                width: 1.5rem;
            }
        `;
    }

}
