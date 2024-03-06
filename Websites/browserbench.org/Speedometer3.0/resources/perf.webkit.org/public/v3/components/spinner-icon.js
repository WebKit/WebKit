
class SpinnerIcon extends ComponentBase {
    constructor()
    {
        super('spinner-icon');
    }

    static cssTemplate()
    {
        return `
        :host {
            display: inline-block;
            width: 2rem;
            height: 2rem;
            will-change: opacity; /* Threre is no will-change: stroke. */
        }
        line {
            animation: spinner-animation 1.6s linear infinite;
            stroke: rgb(230, 230, 230);
            stroke-width: 10;
            stroke-linecap: round;
        }
        line:nth-child(0) { animation-delay: 0.0s; }
        line:nth-child(1) { animation-delay: 0.2s; }
        line:nth-child(2) { animation-delay: 0.4s; }
        line:nth-child(3) { animation-delay: 0.6s; }
        line:nth-child(4) { animation-delay: 0.8s; }
        line:nth-child(5) { animation-delay: 1.0s; }
        line:nth-child(6) { animation-delay: 1.2s; }
        line:nth-child(7) { animation-delay: 1.4s; }
        @keyframes spinner-animation {
            0% { stroke: rgb(25, 25, 25); }
            50% { stroke: rgb(230, 230, 230); }
        }`;
    }

    static htmlTemplate()
    {
        return `<svg class="spinner" viewBox="0 0 100 100">
            <line x1="10" y1="50" x2="30" y2="50"/>
            <line x1="21.72" y1="21.72" x2="35.86" y2="35.86"/>
            <line x1="50" y1="10" x2="50" y2="30"/>
            <line x1="78.28" y1="21.72" x2="64.14" y2="35.86"/>
            <line x1="70" y1="50" x2="90" y2="50"/>
            <line x1="65.86" y1="65.86" x2="78.28" y2="78.28"/>
            <line x1="50" y1="70" x2="50" y2="90"/>
            <line x1="21.72" y1="78.28" x2="35.86" y2="65.86"/>
        </svg>`;
    }

}

ComponentBase.defineElement('spinner-icon', SpinnerIcon);
