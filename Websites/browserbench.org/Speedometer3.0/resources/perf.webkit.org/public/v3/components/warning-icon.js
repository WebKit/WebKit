
class WarningIcon extends ButtonBase {
    constructor()
    {
        super('warning-icon');
    }

    render()
    {
        super.render();
    }

    setWarning(warning)
    {
        this.setButtonTitle(warning);
    }

    static sizeFactor() { return 0.7; }

    static buttonContent()
    {
        return `<g stroke="#9f6000" fill="#9f6000" stroke-width="7">
                <polygon points="0,0, 100,0, 0,100" />
            </g>`;
    }
}

ComponentBase.defineElement('warning-icon', WarningIcon);
