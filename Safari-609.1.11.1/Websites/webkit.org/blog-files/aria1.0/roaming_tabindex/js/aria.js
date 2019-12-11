
/* Aria Singleton */
var Aria = {
  ManagedWidgets: new Array() // instances of Aria.ManagedWidget Class
};

Aria.ManagedWidget = Class.create();
Aria.ManagedWidget.prototype = {
  initialize: function(inNode, inRole, inChildSelector, inChildRole, inAttributes, inFunctionOnSelect){
    if(!$(inNode) && console.error) console.error('Error from aria.js: Aria.ManagedWidget instance initialized with invalid element, '+ inNode);
    
    this.el = $(inNode);
    this.role = inRole;
    this.childSelector = inChildSelector;
    this.childRole = inChildRole;
    this.attributes = inAttributes;
    this.selectFunction = inFunctionOnSelect;
    
    this.index = Aria.ManagedWidgets.length; // each ManagedWidget can know its index in the Aria singleton's instance array
    
    this.el.setAttribute('role', this.role); // set container role (e.g. radiogroup) on the el
    this.childItems = this.el.select(this.childSelector);
    for (var i=0, c=this.childItems.length; i<c; i++) {
      var item = this.childItems[i]; // sets the child role (e.g. radio) on each child
      //item.setAttribute('title', item.innerHTML.stripTags()); // set @title for mouse hover tooltip
      item.setAttribute('role', this.childRole);
      item.setAttribute('tabindex','-1'); // make rest of the items non-focusable
    }
    this.setSelectedItem(false, true); // make the first item selected, but don't focus it
    
    // set up event delegation on the parent node instead of tracking events on all the childItems
    Event.observe(this.el, 'click', this.handleClick.bindAsEventListener(this));
    Event.observe(this.el, 'keydown', this.handleKeyPress.bindAsEventListener(this)); // keypress events inconsistent for arrow keys, so use keydown instead
    
  },
  getNextItem: function(inNode) {
    var el = $(inNode);
    if(el.next()) el = el.next(); // todo: fix this so the items don't have to be DOM siblings
    return el;
  },
  getPreviousItem: function(inNode) {
    var el = $(inNode);
    if(el.previous()) el = el.previous(); // todo: fix this so the items don't have to be DOM siblings
    return el;
  },
  getSelectedItem: function(inNode){
    if(inNode){ // if inNode (from event target), sets the active item to nearest ancestor item
      var el = $(inNode);
      while (el != this.el) {
        if (this.isChildItem(el)) break; // exit the loop; we have the item
        el = el.parentNode;
      }
      if (el != this.el) {
        return this.setSelectedItem(el);
      }
    }
    return this.setSelectedItem(); // set to default
  },
  handleClick: function(inEvent) {
    var target = inEvent.target; // get the click target
    var el = this.getSelectedItem(target);
    this.setSelectedItem(el);
    Event.stop(inEvent); // and stop the event
  },
  handleKeyPress: function(inEvent) {
    switch (inEvent.keyCode) {
      case Event.KEY_DOWN:  this.keyDown();  break;
      case Event.KEY_LEFT:  this.keyLeft();  break;
      case Event.KEY_RIGHT: this.keyRight(); break;
      case Event.KEY_UP:    this.keyUp();    break;
      default: return; // don't cancel keypress unless intercepted successfully
    }
    Event.stop(inEvent);
  },
  isChildItem: function(inNode){
    if (inNode.getAttribute('role') && inNode.getAttribute('role').toLowerCase() == this.childRole) return true;
    else return false;
  },
  keyDown: function() {
    var el = this.selectedItem;
    this.setSelectedItem(this.getNextItem(el));
  },
  keyLeft: function() {
    var el = this.selectedItem;
    this.setSelectedItem(this.getPreviousItem(el));
  },
  keyRight: function() {
    var el = this.selectedItem;
    this.setSelectedItem(this.getNextItem(el));
  },
  keyUp: function() {
    var el = this.selectedItem;
    this.setSelectedItem(this.getPreviousItem(el));
  },
  setSelectedItem: function(inNode, inOptDoNotFocus) {
    if (this.selectedItem){
      this.selectedItem.tabIndex = -1;
      Element.removeClassName(this.selectedItem, 'active');
      // remove the additional attribute assignments on the active item
      for (var i=0, c=this.attributes.length; i<c; i++){
        var attr = this.attributes[i];
        if (attr.falseValue) {
          this.selectedItem.setAttribute(attr.attribute, attr.falseValue); // set each to false
        } else {
          this.selectedItem.removeAttribute(attr.attribute); // or remove it completely
        }
      }
    }
    this.selectedItem = $(inNode) || this.childItems[0];
    Element.addClassName(this.selectedItem, 'active');
    this.selectedItem.tabIndex = 0;
    
    if (this.selectFunction) {
      this.selectFunction(this.childItems);
    }
    
    // add the additional attribute assignments on the active item
    for (var i=0, c=this.attributes.length; i<c; i++){
      var attr = this.attributes[i];
      this.selectedItem.setAttribute(attr.attribute, attr.value);
    }
    
    if (!inOptDoNotFocus) this.selectedItem.focus();
    return this.selectedItem;
  }
};
