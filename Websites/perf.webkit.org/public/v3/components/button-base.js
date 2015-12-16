
class ButtonBase extends ComponentBase {
    constructor(name)
    {
        super(name);
    }

    setCallback(callback)
    {
        this.content().querySelector('a').addEventListener('click', ComponentBase.createActionHandler(callback));
    }

    static cssTemplate()
    {
        return `
            .button {
                vertical-align: bottom;
                display: inline-block;
                width: 1rem;
                height: 1rem;
                opacity: 0.3;
            }

            .button:hover {
                opacity: 0.6;
            }
        `;
    }

}
