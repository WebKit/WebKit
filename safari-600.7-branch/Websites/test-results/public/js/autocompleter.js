function Autocompleter(inputElement, list) {
    this._inputElement = inputElement;
    this._currentSelection = undefined;
    this._list = list;
    this._candidates = [];
    this._currentFilter = null;
    this._candidateWindow = null;

    inputElement.addEventListener('focus', this.show.bind(this));
    inputElement.addEventListener('blur', this.hide.bind(this));
    inputElement.addEventListener('keyup', this.update.bind(this));
    inputElement.addEventListener('keydown', this.navigate.bind(this));
}

Autocompleter.prototype.filterCandidates = function (filter) {
    return this._list.filter(function (testName) { return testName.indexOf(filter) >= 0; });
}

Autocompleter.prototype._ensureCandidateWindow = function () {
    if (this._candidateWindow)
        return;

    var container = element('ul');
    container.className = 'candidateWindow';
    container.style.position = 'absolute';
    container.style.display = 'none';
    this._inputElement.parentNode.appendChild(container);
    this._candidateWindow = container;
}

Autocompleter.prototype._updateCandidates = function (filter) {
    if (this._currentFilter == filter)
        return false;

    var candidates = this.filterCandidates(filter);
    if (candidates.length > 50)
        candidates = [];
    this._candidates = candidates;
    this._currentFilter = filter;
    this._currentSelection = undefined;
    return true;
}

Autocompleter.prototype._showCandidateWindow = function () {
    if (!this._candidateWindow)
        return;
    var style = this._candidateWindow.style;
    style.display = 'block';
    style.top = this._inputElement.offsetTop + this._inputElement.offsetHeight + 'px';
    style.left = this._inputElement.offsetLeft + 'px';
}

Autocompleter.prototype._createItem = function (candidate) {
    var tokens = candidate.split(this._currentFilter);
    var children = [];
    for (var i = 0; i < tokens.length; i++) {
        children.push(text(tokens[i]));
        if (i + 1 < tokens.length)
            children.push(element('em', [this._currentFilter]));
    }
    return element('li', children);
}

Autocompleter.prototype.show = function () {
    this.hide();
    this._ensureCandidateWindow();

    for (var i = 0; i < this._candidates.length; i++)
        this._candidateWindow.appendChild(this._createItem(this._candidates[i]));
    this._selectItem(this._currentSelection);

    if (this._candidates.length)
        this._showCandidateWindow();
}

Autocompleter.prototype.update = function () {
    if (this._updateCandidates(this._inputElement.value))
        this.show();
}

Autocompleter.prototype.hide = function () {
    if (!this._candidateWindow)
        return;
    this._candidateWindow.style.display = 'none';
    this._candidateWindow.innerHTML = '';
}

Autocompleter.prototype._selectItem = function (index) {
    if (!this._candidateWindow || this._currentSelection == index)
        return;

    var item = this._candidateWindow.childNodes[index];
    if (!item)
        return;
    item.classList.add('selected');

    var oldItem = this._candidateWindow.childNodes[this._currentSelection];
    if (oldItem)
        oldItem.classList.remove('selected');

    this._currentSelection = index;
}

Autocompleter.prototype.navigate = function (event) {
    if (event.keyCode == 0x28 /* DOM_VK_DOWN */) {
        this._selectItem(this._currentSelection === undefined ? 0 : Math.min(this._currentSelection + 1, this._candidates.length - 1));
        event.preventDefault();
    } else if (event.keyCode == 0x26 /* DOM_VK_UP */) {
        this._selectItem(this._currentSelection === undefined ? this._candidates.length - 1 : Math.max(this._currentSelection - 1, 0));
        event.preventDefault();
    } else if (event.keyCode == 0x0D /* VK_RETURN */) {
        if (this._currentSelection === undefined)
            return;
        this._inputElement.value = this._candidates[this._currentSelection];
    } else if (event.keyCode == 0x1B /* DOM_VK_ESCAPE */) {
        this.hide();
    }
}
