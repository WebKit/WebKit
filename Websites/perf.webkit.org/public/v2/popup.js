App.PopupView = Ember.View.extend({
    templateName: 'popup',
    classNames: ['popup'],
    popupListContainerView: Ember.ContainerView.extend(),
    init: function ()
    {
        this._super();
        this._listView = null;
    },
    showPopup: function ()
    {
        this.unscheduleHiding();
        if (!this._listView) {
            var list = this.get('list');
            this._listView = App.PopupListView.create({tagName: 'ul', list: list});
            this.get('popupListContainerViewInstance').pushObject(this._listView);
        }
        this._listView.showList();
    },
    scheduleHiding: function ()
    {
        this._hidingTimer = Ember.run.later(this, this.hideNow, 500);
    },
    unscheduleHiding: function ()
    {
        Ember.run.cancel(this._hidingTimer);
        this._hidingTimer = null;
    },
    hideNow: function ()
    {
        if (this._listView)
            this._listView.hideList();
    }
});

App.PopupButtonView = Ember.View.extend({
    template: Ember.Handlebars.compile('{{view.label}}'),
    classNames: ['popup-button'],
    attributeBindings: ['href'],
    href: '#',
    mouseEnter: function ()
    {
        this.get('parentView').showPopup();
    },
    click: function (event)
    {
        this.get('parentView').showPopup();
        event.preventDefault();
    },
    mouseLeave: function ()
    {
        this.get('parentView').scheduleHiding();
    },
});

App.PopupListView = Ember.View.extend({
    templateName: 'popup-list',
    init: function ()
    {
        this._super();
        this._shouldHide = true;
    },
    _poupView: function ()
    {
        var containerView = this.get('parentView');
        return containerView.get('parentView');
    },
    didInsertElement: function ()
    {
        if (this._shouldHide)
            this.$().hide();
        else
            this._showListSyncIfPossible();
        this._super();
    },
    mouseEnter: function ()
    {
        this._poupView().unscheduleHiding();
    },
    mouseLeave: function ()
    {
        this._poupView().scheduleHiding();
    },
    showList: function ()
    {
        this._shouldHide = false;
        this._showListSyncIfPossible();
    },
    _showListSyncIfPossible: function ()
    {
        if (!this.$())
            return;
        var popupView = this._poupView();
        if (popupView.get('parentView') instanceof App.PopupListView) {
            var parentListView = popupView.get('parentView');
            parentListView.get('childViews').map(function (sibling) {
                if (sibling != popupView)
                    sibling.hideNow();
            });
            this.$().css('left', parentListView.$().outerWidth() + 5);
        } else
            this.$().css('top', popupView.$().height() - 5);
        this.$().show();
    },
    hideList: function ()
    {
        this._shouldHide = true;
        this._hideListSyncIfPossible();
    },
    _hideListSyncIfPossible: function ()
    {
        if (!this.$())
            return;
        this.$().hide();
    }
});
