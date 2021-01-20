
function loadWidgets(){
  
  // init the radiogroup
  $$('.segmentedcontrol').each(function(elm){
    Aria.ManagedWidgets.push(
      new Aria.ManagedWidget(
        elm, 'radiogroup', 'li', 'radio', [
          { 'attribute':'aria-checked', 'value':'true', 'falseValue':'false' } // mark the active item as 'checked' (attribute toggles between value and falseValue)
        ],
        // one-off function to switch list className when view selection changes
        function (inChildItems) {
          var list = $$('.itemlist')[0];
          for (var i=0, c=inChildItems.length; i<c; i++) {
            var activeItem = inChildItems[i];
            if (Element.hasClassName(activeItem, 'active')) {
              list.className = 'itemlist ' + activeItem.id.replace('radio_','style_');
              break;
            }
          }
        }
      )
    );
  });
  
  // init the listbox
  $$('.itemlist').each(function(elm){
    Aria.ManagedWidgets.push(
      new Aria.ManagedWidget(
        elm, 'listbox', 'li', 'option', [
          { 'attribute':'aria-selected', 'value':'true' } // mark the active item as 'selected' (no falseValue, so attribute toggles between being set to value or removed entirely)
        ]
      )
    );
  });
  
}

Event.observe(window, 'load', loadWidgets); // may want to use DOMContentLoaded instead of load

