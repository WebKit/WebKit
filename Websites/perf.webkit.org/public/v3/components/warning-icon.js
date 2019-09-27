
class WarningIcon extends ButtonBase {
    constructor(warningMessage)
    {
        super('warning-icon');
        this._warningMessage = warningMessage;
    }

    render()
    {
        super.render();
        if (this._warningMessage)
            this.setButtonTitle(this._warningMessage);
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
