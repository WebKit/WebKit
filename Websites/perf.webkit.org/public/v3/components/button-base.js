
class ButtonBase extends ComponentBase {
    constructor(name)
    {
        super(name);
    }

    setCallback(callback)
    {
        this.content().querySelector('a').addEventListener('click', ComponentBase.createActionHandler(callback));
    }

    static htmlTemplate()
    {
        return `<a class="button" href="#"><svg viewBox="0 0 100 100">${this.buttonContent()}</svg></a>`;
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

            .button {
                vertical-align: bottom;
                display: block;
                opacity: 0.3;
            }

            .button svg {
                display: block;
            }

            .button:hover {
                opacity: 0.6;
            }
        `;
    }
}
