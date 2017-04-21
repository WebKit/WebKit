
class TestGroupForm extends ComponentBase {

    constructor(name)
    {
        super(name || 'test-group-form');
        this._repetitionCount = 4;
    }

    setRepetitionCount(count)
    {
        this.content('repetition-count').value = count;
        this._repetitionCount = count;
    }

    didConstructShadowTree()
    {
        const repetitionCountSelect = this.content('repetition-count');
        repetitionCountSelect.onchange = () => {
            this._repetitionCount = repetitionCountSelect.value;
        }
        this.content('form').onsubmit = this.createEventHandler(() => this.startTesting());
    }

    startTesting()
    {
        this.dispatchAction('startTesting', this._repetitionCount);
    }

    static htmlTemplate()
    {
        return `<form id="form"><button id="start-button" type="submit"><slot>Start A/B testing</slot></button>${this.formContent()}</form>`;
    }

    static cssTemplate()
    {
        return `
            :host {
                display: block;
            }
        `;
    }

    static formContent()
    {
        return `
            with
            <select id="repetition-count">
                <option>1</option>
                <option>2</option>
                <option>3</option>
                <option selected>4</option>
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
