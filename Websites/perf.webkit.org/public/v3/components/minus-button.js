
class MinusButton extends ButtonBase {
    constructor()
    {
        super('minus-button');
    }

    static buttonContent()
    {
        return `<g stroke="black" stroke-width="10" id="icon">
            <circle cx="50" cy="50" r="40" fill="transparent"/>
            <polygon points="25,50 75,50" />
        </g>`;
    }
}

ComponentBase.defineElement('minus-button', MinusButton);
