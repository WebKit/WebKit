
class SpinnerIcon extends ComponentBase {
    constructor()
    {
        super('spinner-icon');
    }

    static cssTemplate()
    {
        return `
        .spinner {
            width: 2rem;
            height: 2rem;
            -webkit-transform: translateZ(0);
        }
        .spinner line {
            animation: spinner-animation 1.6s linear infinite;
            -webkit-animation: spinner-animation 1.6s linear infinite;
            opacity: 0.1;
        }
        .spinner line:nth-child(0) {
            -webkit-animation-delay: 0.0s;
            animation-delay: 0.0s;
        }
        .spinner line:nth-child(1) {
            -webkit-animation-delay: 0.2s;
            animation-delay: 0.2s;
        }
        .spinner line:nth-child(2) {
            -webkit-animation-delay: 0.4s;
            animation-delay: 0.4s;
        }
        .spinner line:nth-child(3) {
            -webkit-animation-delay: 0.6s;
            animation-delay: 0.6s;
        }
        .spinner line:nth-child(4) {
            -webkit-animation-delay: 0.8s;
            animation-delay: 0.8s;
        }
        .spinner line:nth-child(5) {
            -webkit-animation-delay: 1s;
            animation-delay: 1s;
        }
        .spinner line:nth-child(6) {
            -webkit-animation-delay: 1.2s;
            animation-delay: 1.2s;
        }
        .spinner line:nth-child(7) {
            -webkit-animation-delay: 1.4s;
            animation-delay: 1.4s;
        }
        .spinner line:nth-child(8) {
            -webkit-animation-delay: 1.6s;
            animation-delay: 1.6s;
        }
        @keyframes spinner-animation {
            0% { opacity: 0.9; }
            50% { opacity: 0.1; }
            100% { opacity: 0.1; }
        }
        @-webkit-keyframes spinner-animation {
            0% { opacity: 0.9; }
            50% { opacity: 0.1; }
            100% { opacity: 0.1; }
        }
        `;
    }

    static htmlTemplate()
    {
        return `<svg class="spinner" viewBox="0 0 100 100">
            <line x1="10" y1="50" x2="30" y2="50" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="21.72" y1="21.72" x2="35.86" y2="35.86" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="50" y1="10" x2="50" y2="30" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="78.28" y1="21.72" x2="64.14" y2="35.86" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="70" y1="50" x2="90" y2="50" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="65.86" y1="65.86" x2="78.28" y2="78.28" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="50" y1="70" x2="50" y2="90" stroke="black" stroke-width="10" stroke-linecap="round"/>
            <line x1="21.72" y1="78.28" x2="35.86" y2="65.86" stroke="black" stroke-width="10" stroke-linecap="round"/>
        </svg>`;
    }

}

ComponentBase.defineElement('spinner-icon', SpinnerIcon);
