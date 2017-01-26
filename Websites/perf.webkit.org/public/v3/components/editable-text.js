
class EditableText extends ComponentBase {

    constructor(text)
    {
        super('editable-text');
        this._text = text;
        this._inEditingMode = false;
        this._updatingPromise = null;
        this._actionLink = this.content().querySelector('.editable-text-action a');
        this._label = this.content().querySelector('.editable-text-label');
    }

    didConstructShadowTree()
    {
        const button = this.content('action-button');
        button.addEventListener('mousedown', this.createEventHandler(() => this._didClick()));
        button.addEventListener('click', this.createEventHandler(() => this._didClick()));
        this.element().addEventListener('blur',() => {
            if (!this.content().activeElement)
                this._endEditingMode();
        });
    }

    editedText() { return this.content('text-field').value; }
    text() { return this._text; }
    setText(text)
    {
        this._text = text;
        this.enqueueToRender();
    }

    render()
    {
        const label = this.content('label');
        label.textContent = this._text;
        label.style.display = this._inEditingMode ? 'none' : null;

        const textField = this.content('text-field');
        textField.style.display = this._inEditingMode ? null : 'none';
        textField.readOnly = !!this._updatingPromise;

        this.content('action-button').textContent = this._inEditingMode ? (this._updatingPromise ? '...' : 'Save') : 'Edit';
        this.content('action-button-container').style.display = this._text ? null : 'none';

        if (this._inEditingMode) {
            if (!this._updatingPromise)
                textField.focus();
        }
    }

    _didClick()
    {
        if (this._updatingPromise)
            return;

        if (this._inEditingMode) {
            const result = this.dispatchAction('update');
            if (result instanceof Promise)
                this._updatingPromise = result.then(() => this._endEditingMode());
            else
                this._endEditingMode();
        } else {
            this._inEditingMode = true;
            const textField = this.content('text-field');
            textField.value = this._text;
            textField.style.width = (this._text.length / 1.5) + 'rem';
            this.enqueueToRender();
        }
    }

    _endEditingMode()
    {
        this._inEditingMode = false;
        this._updatingPromise = null;
        this.enqueueToRender();
    }

    static htmlTemplate()
    {
        return `
            <span id="container">
                <input id="text-field" type="text">
                <span id="label"></span>
                <span id="action-button-container">(<a id="action-button" href="#">Edit</a>)</span>
            </span>`;
    }

    static cssTemplate()
    {
        return `
            #container {
                position: relative;
                padding-right: 2.5rem;
            }
            #text-field {
                background: transparent;
                margin: 0;
                padding: 0;
                color: inherit;
                font-weight: inherit;
                font-size: inherit;
                border: none;
            }
            #action-button-container {
                position: absolute;
                right: 0;
                padding-left: 0.2rem;
                color: #999;
                font-size: 0.8rem;
                top: 50%;
                margin-top: -0.4rem;
                vertical-align: middle;
            }
            #action-button {
                color: inherit;
                text-decoration: none;
            }
        `;
    }
}

ComponentBase.defineElement('editable-text', EditableText);
