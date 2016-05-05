
class WarningIcon extends ButtonBase {
    constructor()
    {
        super('warning-icon');
    }

    static cssTemplate()
    {
        return super.cssTemplate() + `
            .button {
                display: block;
                width: 0.7rem;
                height: 0.7rem;
            }
            .button svg {
                display: block;
            }
        `;
    }

    static htmlTemplate()
    {
        return `<a class="button" href="#"><svg viewBox="0 0 100 100">
            <g stroke="#9f6000" fill="#9f6000" stroke-width="7">
                <polygon points="0,0, 100,0, 0,100" />
            </g>
        </svg></a>`;
    }

}

ComponentBase.defineElement('warning-icon', WarningIcon);
