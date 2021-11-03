
class TestGroupForm extends ComponentBase {

    constructor(name)
    {
        super(name || 'test-group-form');
        this._repetitionCount = 4;
        this._notifyOnCompletion = true;
    }

    setRepetitionCount(count)
    {
        this.content('repetition-count').value = count;
        this._repetitionCount = count;
    }

    setTestAndPlatform(test, platform)
    {
        this.part('repetition-type-selector').setTestAndPlatform(test, platform);
    }

    updateWithTestGroup(testGroup)
    {
        console.assert(testGroup);
        this.setRepetitionCount(testGroup.initialRepetitionCount());
        this.setTestAndPlatform(testGroup.test(), testGroup.platform());
        this.part('repetition-type-selector').selectedRepetitionType = testGroup.repetitionType();
    }

    didConstructShadowTree()
    {
        const repetitionCountSelect = this.content('repetition-count');
        repetitionCountSelect.onchange = () => {
            this._repetitionCount = repetitionCountSelect.value;
        };
        const notifyOnCompletionCheckBox = this.content('notify-on-completion-checkbox');
        notifyOnCompletionCheckBox.onchange = () => this._notifyOnCompletion = notifyOnCompletionCheckBox.checked;
        this.content('form').onsubmit = this.createEventHandler(() => this.startTesting());
    }

    startTesting()
    {
        const repetitionType = this.part('repetition-type-selector').selectedRepetitionType;
        this.dispatchAction('startTesting', this._repetitionCount, repetitionType, this._notifyOnCompletion);
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

            #notify-on-completion-checkbox {
                margin-left: 0.5rem;
                width: 1rem;
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
            <label>iterations per set in</label>
            <repetition-type-selection id="repetition-type-selector"></repetition-type-selection>
            <input id="notify-on-completion-checkbox" type="checkbox" checked/>Notify on completion
        `;
    }

}

ComponentBase.defineElement('test-group-form', TestGroupForm);
