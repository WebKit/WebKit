description('A unselected option was submitted as fallback. This behavior was removed by the change of webkit.org/b/35056.');

var parent = document.createElement('div');
document.body.appendChild(parent);
parent.innerHTML = '<form action="">'
    + '<input type=hidden name="submitted" value="true">'
    + '<select name="select">'
    + '<option disabled>Disabled</option>'
    + '</select>'
    + '</form>';

if (window.layoutTestController)
    layoutTestController.waitUntilDone();
var query = window.location.search;
if (query.indexOf('submitted=true') == -1) {
    var select = document.getElementsByTagName('select')[0];
    select.selectedIndex = 0;
    document.forms[0].submit();
} else {
    shouldBe('query.indexOf("select=Disabled")', '-1');
    if (window.layoutTestController)
        layoutTestController.notifyDone();
}
