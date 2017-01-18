
class ChartsToolbar extends DomainControlToolbar {
    constructor()
    {
        super('chars-toolbar', 7);

        this._minDayCount = 1;
        this._maxDayCount = 366;

        this._numberOfDaysCallback = null;
        this._slider = this.content().querySelector('.slider');
        this._slider.addEventListener('change', this._sliderValueMayHaveChanged.bind(this));
        this._slider.addEventListener('mousemove', this._sliderValueMayHaveChanged.bind(this));

        this._editor = this.content().querySelector('.editor');
        this._editor.addEventListener('focus', this._enterTextMode.bind(this));
        this._editor.addEventListener('blur', this._exitTextMode.bind(this));
        this._editor.addEventListener('input', this._editorValueMayHaveChanged.bind(this));
        this._editor.addEventListener('change', this._editorValueMayHaveChanged.bind(this));

        this._labelSpan = this.content().querySelector('.day-count');

        this._addPaneCallback = null;
        this._paneSelector = this.content().querySelector('pane-selector').component();
        this._paneSelectorOpener = this.content().querySelector('.pane-selector-opener');
        this._paneSelectorContainer = this.content().querySelector('.pane-selector-container');

        this._paneSelector.setCallback(this._addPane.bind(this));
        this._paneSelectorOpener.addEventListener('click', this._togglePaneSelector.bind(this));

        this._paneSelectorContainer.style.display = 'none';
    }

    render()
    {
        super.render();
        this._paneSelector.enqueueToRender();
        this._labelSpan.textContent = this._numberOfDays;
        this._setInputElementValue(this._numberOfDays);
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
        this._exitTextMode();
        if (startTime)
            super.setStartTime(startTime);
        else
            super.setNumberOfDays(7);
    }

    _setInputElementValue(value)
    {
        this._slider.value = Math.pow(value, 1/3);
        this._slider.min = Math.pow(this._minDayCount, 1/3);
        this._slider.max = Math.pow(this._maxDayCount, 1/3);
        this._slider.step = 'any';
        this._editor.value = value;
    }

    _enterTextMode(event)
    {
        event.preventDefault();
        this._editor.style.opacity = 1;
        this._editor.style.marginLeft = '-2.5rem';
        this._labelSpan.style.opacity = 0;
        this._slider.style.opacity = 0;
    }

    _exitTextMode(event)
    {
        if (event)
            event.preventDefault();
        this._editor.style.opacity = 0;
        this._editor.style.marginLeft = null;
        this._labelSpan.style.opacity = 1;
        this._slider.style.opacity = 1;
    }

    _sliderValueMayHaveChanged(event)
    {
        var numberOfDays = Math.round(Math.pow(parseFloat(this._slider.value), 3));
        this._callNumberOfDaysCallback(event, numberOfDays);
    }

    _editorValueMayHaveChanged(event)
    {
        var rawNumber = Math.round(parseFloat(this._editor.value));
        var numberOfDays = Math.max(this._minDayCount, Math.min(this._maxDayCount, rawNumber));
        if (this._editor.value != numberOfDays)
            this._editor.value = numberOfDays;
        this._callNumberOfDaysCallback(event, numberOfDays);
    }

    _callNumberOfDaysCallback(event, numberOfDays)
    {
        var shouldUpdateState = event.type == 'change';
        if ((this.numberOfDays() != numberOfDays || shouldUpdateState) && this._numberOfDaysCallback)
            this._numberOfDaysCallback(numberOfDays, shouldUpdateState);
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
        if (this._addPaneCallback)
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
                    <li class="start-time-slider">
                        <label>
                            <input class="slider" type="range">
                            <input class="editor" type="number">
                            <span class="label"><span class="day-count" tabindex="0">?</span> days</span>
                        </label>
                    </li>
                </ul>
                <div class="pane-selector-container">
                    <pane-selector></pane-selector>
                </div>
            </nav>`;
    }

    static cssTemplate()
    {
        return Toolbar.cssTemplate() + `

            .charts-toolbar > .buttoned-toolbar:first-child {
                margin-right: 0.5rem;
            }

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

            .buttoned-toolbar .start-time-slider {
                margin-left: 2rem;
                line-height: 1em;
                font-size: 0.9rem;
            }

            .start-time-slider label {
                display: inline-block;
            }

            .start-time-slider .slider {
                height: 0.8rem;
            }

            .start-time-slider .editor {
                position: absolute;
                opacity: 0;
                width: 4rem;
                font-weight: inherit;
                font-size: 0.8rem;
                height: 0.9rem;
                outline: none;
                border: solid 1px #ccc;
                z-index: 5;
            }

            .start-time-slider .day-count {
                display: inline-block;
                text-align: right;
                width: 2rem;
            }
        `;
    }

}
