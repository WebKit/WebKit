
class DashboardToolbar extends DomainControlToolbar {
    constructor()
    {
        var options = [
            {label: '1D', days: 1},
            {label: '1W', days: 7},
            {label: '2W', days: 14},
            {label: '1M', days: 30},
            {label: '3M', days: 90},
        ];
        super('dashboard-toolbar', options[1].days);
        this._options = options;
        this._currentOption = this._options[1];
    }

    setNumberOfDays(days)
    {
        if (!days)
            days = 7;
        this._currentOption = this._options[this._options.length - 1];
        for (var option of this._options) {
            if (days <= option.days) {
                this._currentOption = option;
                break;
            }
        }
        super.setNumberOfDays(this._currentOption.days);
    }

    render()
    {
        console.assert(this.router());

        var currentPage = this.router().currentPage();
        console.assert(currentPage);
        console.assert(currentPage instanceof DashboardPage);

        super.render();

        var element = ComponentBase.createElement;
        var link = ComponentBase.createLink;

        var self = this;
        var router = this.router();
        var currentOption = this._currentOption;
        this.renderReplace(this.content().querySelector('.dashboard-toolbar'),
            element('ul', {class: 'buttoned-toolbar'},
                this._options.map(function (option) {
                    return element('li', {
                        class: option == currentOption ? 'selected' : '',
                    }, link(option.label, router.url(currentPage.routeName(), {numberOfDays: option.days})));
                })
            ));
    }

    static htmlTemplate()
    {
        return `<div class="dashboard-toolbar"></div>`;
    }

}
