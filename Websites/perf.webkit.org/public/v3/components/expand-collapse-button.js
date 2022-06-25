
class ExpandCollapseButton extends ButtonBase {
    constructor()
    {
        super('expand-collapse-button');
        this._expanded = false;
    }

    didConstructShadowTree()
    {
        super.didConstructShadowTree();
        this.listenToAction('activate', () => {
            this._expanded = !this._expanded;
            this.content('button').className = this._expanded ? 'expanded' : null;
            this.enqueueToRender();
            this.dispatchAction('toggle', this._expanded);
        });
    }

    static sizeFactor() { return 0.8; }

    static buttonContent()
    {
        return `<g stroke="#333" fill="none" stroke-width="10">
            <polyline points="0,25 50,50 100,25"/>
            <polyline points="0,50 50,75 100,50"/>
        </g>`;
    }

    static cssTemplate()
    {
        return super.cssTemplate() + `
            a.expanded {
                transform: rotate(180deg);
            }
        `;
    }
}

ComponentBase.defineElement('expand-collapse-button', ExpandCollapseButton);