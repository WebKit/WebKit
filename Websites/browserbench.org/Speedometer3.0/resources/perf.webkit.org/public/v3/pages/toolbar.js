
class Toolbar extends ComponentBase {

    constructor(name)
    {
        super(name);
        this._router = null;
    }

    router() { return this._router; }
    setRouter(router) { this._router = router; }

    static cssTemplate()
    {
        return `
            .buttoned-toolbar {
                display: inline-block;
                margin: 0 0;
                padding: 0;
                font-size: 0.9rem;
                border-radius: 0.5rem;
            }

            .buttoned-toolbar li > input {
                margin: 0;
                border: solid 1px #ccc;
                border-radius: 0.5rem;
                outline: none;
                font-size: inherit;
                padding: 0.2rem 0.3rem;
                height: 1rem;
            }

            .buttoned-toolbar > input,
            .buttoned-toolbar > ul {
                margin-left: 0.5rem;
            }

            .buttoned-toolbar li {
                display: inline;
                margin: 0;
                padding: 0;
            }

            .buttoned-toolbar li a {
                display: inline-block;
                border: solid 1px #ccc;
                font-weight: inherit;
                text-decoration: none;
                text-transform: capitalize;
                background: #fff;
                color: inherit;
                margin: 0;
                margin-right: -1px; /* collapse borders between two buttons */
                padding: 0.2rem 0.3rem;
            }

            .buttoned-toolbar input:focus,
            .buttoned-toolbar li.selected a {
                background: rgba(204, 153, 51, 0.1);
            }

            .buttoned-toolbar li:not(.selected) a:hover {
                background: #eee;
            }

            .buttoned-toolbar li:first-child a {
                border-top-left-radius: 0.3rem;
                border-bottom-left-radius: 0.3rem;
            }

            .buttoned-toolbar li:last-child a {
                border-right-width: 1px;
                border-top-right-radius: 0.3rem;
                border-bottom-right-radius: 0.3rem;
            }`;
    }
}
