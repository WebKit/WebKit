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

    var payload;
    if (this.payload)
        payload = this.payload.value;
    else {
        var contents = {};
        for (var i = 0; i < this.elements.length; i++)
            contents[this.elements[i].name] = this.elements[i].value;
        payload = JSON.stringify(contents);
    }

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
    xhr.send(payload);

    $(this).trigger('reload');
});

$('#manual-submission textarea').val(JSON.stringify({
    'branch': 'webkit-trunk',
    'platform': 'chromium-mac',
    'builder-name': 'Chromium Mac Release (Perf)',
    'build-number': '123',
    'timestamp': parseInt(Date.now() / 1000),
    'webkit-revision': 104856,
    'chromium-revision': 123059,
    'results':
        {
            'webkit_style_test': {'avg': 100, 'median': 102, 'stdev': 5, 'min': 90, 'max': 110},
            'some_test': 54,
        },
}, null, '  '));
