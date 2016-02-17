
class TestGroupForm extends ComponentBase {

    constructor()
    {
        super('test-group-form');
        this._startCallback = null;
        this._repetitionCountControl = this.content().querySelector('.repetition-count');
        this._repetitionCountControl.value = 4;
        this._buttonControl = this.content().querySelector('button');
        this._nameControl = this.content().querySelector('.name');
        this.content().querySelector('form').onsubmit = this._submitted.bind(this);
    }

    setStartCallback(callback) { this._startCallback = callback; }
    setNeedsName(needsName) { this._nameControl.style.display = needsName ? null : 'none'; }
    setDisabled(disabled) { this._buttonControl.disabled = disabled; }

    setLabel(label) { this._buttonControl.textContent = label; }
    setRepetitionCount(count) { this._repetitionCountControl.value = count; }

    _submitted(event)
    {
        event.preventDefault();
        if (this._startCallback)
            this._startCallback(this._nameControl.value, this._repetitionCountControl.value);
    }

    static htmlTemplate()
    {
        return `
            <form>
                <button type="submit">Start A/B testing</button>
                <input class="name" type="text">
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
            </form>
        `;
    }

}

ComponentBase.defineElement('test-group-form', TestGroupForm);
