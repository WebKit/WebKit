
class TestGroupForm extends ComponentBase {

    constructor(name)
    {
        super(name || 'test-group-form');
        this._startCallback = null;
        this._disabled = false;
        this._label = undefined;
        this._repetitionCount = 4;

        this._nameControl = this.content().querySelector('.name');
        this._repetitionCountControl = this.content().querySelector('.repetition-count');
        var self = this;
        this._repetitionCountControl.onchange = function () {
            self._repetitionCount = self._repetitionCountControl.value;
        }

        var boundSubmitted = this._submitted.bind(this);
        this.content().querySelector('form').onsubmit = function (event) {
            event.preventDefault();
            boundSubmitted();
        }
    }

    setStartCallback(callback) { this._startCallback = callback; }
    setDisabled(disabled) { this._disabled = !!disabled; }
    setLabel(label) { this._label = label; }
    setRepetitionCount(count) { this._repetitionCount = count; }

    render()
    {
        var button = this.content().querySelector('button');
        if (this._label)
            button.textContent = this._label;
        button.disabled = this._disabled;
        this._repetitionCountControl.value = this._repetitionCount;
    }

    _submitted()
    {
        if (this._startCallback)
            this._startCallback(this._repetitionCount);
    }

    static htmlTemplate()
    {
        return `<form><button type="submit">Start A/B testing</button>${this.formContent()}</form>`;
    }

    static formContent()
    {
        return `
            with
            <select class="repetition-count">
                <option>1</option>
                <option>2</option>
                <option>3</option>
                <option>4</option>
                <option>5</option>
                <option>6</option>
                <option>7</option>
                <option>8</option>
                <option>9</option>
                <option>10</option>
            </select>
            iterations per set
        `;
    }

}

ComponentBase.defineElement('test-group-form', TestGroupForm);
