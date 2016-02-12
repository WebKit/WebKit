
class EditableText extends ComponentBase {

    constructor(text)
    {
        super('editable-text');
        this._text = text;
        this._inEditingMode = false;
        this._startedEditingCallback = null;
        this._updateCallback = null;
        this._updatingPromise = null;
        this._actionLink = this.content().querySelector('.editable-text-action a');
        this._actionLink.onclick = this._didClick.bind(this);
        this._actionLink.onmousedown = this._didClick.bind(this);
        this._textField = this.content().querySelector('.editable-text-field');
        this._textField.onblur = this._didBlur.bind(this);
        this._label = this.content().querySelector('.editable-text-label');
    }

    editedText() { return this._textField.value; }
    setText(text) { this._text = text; }

    setStartedEditingCallback(callback) { this._startedEditingCallback = callback; }
    setUpdateCallback(callback) { this._updateCallback = callback; }

    render()
    {
        this._label.textContent = this._text;
        this._actionLink.textContent = this._inEditingMode ? (this._updatingPromise ? '...' : 'Save') : 'Edit';
        this._actionLink.parentNode.style.display = this._text ? null : 'none';

        if (this._inEditingMode) {
            this._textField.readOnly = !!this._updatingPromise;
            this._textField.style.display = null;
            this._label.style.display = 'none';
            if (!this._updatingPromise)
                this._textField.focus();
        } else {
            this._textField.style.display = 'none';
            this._label.style.display = null;
        }

        super.render();
    }

    _didClick(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (!this._updateCallback || this._updatingPromise)
            return;

        if (this._inEditingMode)
            this._updatingPromise = this._updateCallback().then(this._didUpdate.bind(this));
        else {
            this._inEditingMode = true;
            this._textField.value = this._text;
            this._textField.style.width = (this._text.length / 1.5) + 'rem';
            if (this._startedEditingCallback)
                this._startedEditingCallback();
        }
    }

    _didBlur(event)
    {
        var self = this;
        if (self._inEditingMode && !self._updatingPromise && !self.hasFocus())
            self._didUpdate();
    }

    _didUpdate()
    {
        this._inEditingMode = false;
        this._updatingPromise = null;
        this.render();
    }

    static htmlTemplate()
    {
        return `
            <span class="editable-text-container">
                <input type="text" class="editable-text-field">
                <span class="editable-text-label"></span>
                <span class="editable-text-action">(<a href="#">Edit</a>)</span>
            </span>`;
    }

    static cssTemplate()
    {
        return `
            .editable-text-container {
                position: relative;
                padding-right: 2.5rem;
            }
            .editable-text-field {
                background: transparent;
                margin: 0;
                padding: 0;
                color: inherit;
                font-weight: inherit;
                font-size: inherit;
                width: 8rem;
                border: none;
            }
            .editable-text-action {
                position: absolute;
                padding-left: 0.2rem;
                color: #999;
                font-size: 0.8rem;
                top: 50%;
                margin-top: -0.4rem;
                vertical-align: middle;
            }
            .editable-text-action a {
                color: inherit;
                text-decoration: none;
            }
        `;
    }
}

ComponentBase.defineElement('editable-text', EditableText);
