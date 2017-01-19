
class ButtonBase extends ComponentBase {
    didConstructShadowTree()
    {
        this.content('button').addEventListener('click', this.createEventHandler(() => {
            this.dispatchAction('activate');
        }));
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

            svg {
                display: block;
            }
        `;
    }
}
