
class CloseButton extends ButtonBase {
    constructor()
    {
        super('close-button');
    }

    static htmlTemplate()
    {
        return `
            <a class="button" href="#"><svg viewBox="0 0 100 100">
                <g stroke="black" stroke-width="10">
                    <circle cx="50" cy="50" r="45" fill="transparent"/>
                    <polygon points="30,30 70,70" />
                    <polygon points="30,70 70,30" />
                </g>
            </svg></a>`;
    }
}

ComponentBase.defineElement('close-button', CloseButton);
