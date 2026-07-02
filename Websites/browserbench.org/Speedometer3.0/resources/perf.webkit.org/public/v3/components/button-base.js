
class ButtonBase extends ComponentBase {

    constructor(name)
    {
        super(name);
        this._disabled = false;
        this._title = null;
    }

    setDisabled(disabled)
    {
        this._disabled = disabled;
        this.enqueueToRender();
    }

    setButtonTitle(title)
    {
        this._title = title;
        this.enqueueToRender();
    }

    didConstructShadowTree()
    {
        this.content('button').addEventListener('click', this.createEventHandler(() => {
            this.dispatchAction('activate');
        }));
    }

    render()
    {
        super.render();
        if (this._disabled)
            this.content('button').setAttribute('disabled', '');
        else
            this.content('button').removeAttribute('disabled');
        if (this._title)
            this.content('button').setAttribute('title', this._title);
        else
            this.content('button').removeAttribute('title');
    }

    static htmlTemplate()
    {
        return `<a id="button" href="#"><svg viewBox="0 0 100 100">${this.buttonContent()}</svg></a>`;
    }

    static buttonContent() { throw 'NotImplemented'; }
    static sizeFactor() { return 1; }

    static cssTemplate()
    {
        const sizeFactor = this.sizeFactor();
        return `
            :host {
                display: inline-block;
                width: ${sizeFactor}rem;
                height: ${sizeFactor}rem;
            }

            a {
                vertical-align: bottom;
                display: block;
                opacity: 0.3;
            }

            a:hover {
                opacity: 0.6;
            }

            a[disabled] {
                pointer-events: none;
                cursor: default;
                opacity: 0.2;
            }

            svg {
                display: block;
            }
        `;
    }
}
