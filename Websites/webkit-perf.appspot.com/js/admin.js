function removeNonFormListItems(list) {
    list.children().each(function () {
        if ($.inArray('form', this.classList))
            $(this).remove();
    });
}

function createKeyNameReloader(name) {
    return function () {
        $.getJSON(name, function (platforms) {
            var list = $('#' + name + ' ul');
            removeNonFormListItems(list);
            $.each(platforms, function (key, name) {
                list.append('<li>' + key + ' : ' + name + '</li>');
            });
            list.append($('#' + name + ' ul .form'));
        });
    }
}

$('#branches form').bind('reload', createKeyNameReloader('branches'));
$('#platforms form').bind('reload', createKeyNameReloader('platforms'));

$('#builders form').bind('reload', function () {
    $.getJSON('builders', function (builders) {
        var list = $('#builders ul');
        removeNonFormListItems(list);
        builders = builders.sort();
        for (var i = 0; i < builders.length; i++)
            list.append('<li><a href="http://build.webkit.org/builders/' + builders[i] + '">' + builders[i] + '</a></li>');
        list.append($('#builders ul .form'));
    });
});

$('#tests form').bind('reload', function () {
    $.getJSON('tests', function (tests) {
        var list = $('#tests ul');
        removeNonFormListItems(list);
        var select = $('#tests select');
        select.children().remove();

        tests = tests.sort();
        for (var i = 0; i < tests.length; i++) {
            list.append('<li>' + tests[i] + '</li>'); // FIXME: Add a link to trac page.
            select.append('<option value="' + tests[i] + '">' + tests[i] + '</option>');
        }

        list.append($('#tests ul .form'));
    });
});

$.ajaxSetup({
    'error': function(xhr, e, message) { console.log(xhr); error('Failed with HTTP status: ' + xhr.status, e); },
    cache: true,
});

$('form').trigger('reload');

$('form').bind('submit', function (event) {
    event.preventDefault();

    var contents = {}
    for (var i = 0; i < this.elements.length; i++)
        contents[this.elements[i].name] = this.elements[i].value;

    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function () {
        if (xhr.readyState != 4)
            return;
        if (xhr.status != 200)
            error('HTTP status: ' + xhr.status);
        else if (xhr.responseText != 'OK')
            error(xhr.responseText);
        
    }
    xhr.open(this.method, this.action, true);
    xhr.send(JSON.stringify(contents));

    $(this).trigger('reload');
});
