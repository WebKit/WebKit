function submitXHR(method, action, payload, callback) {
    var xhr = new XMLHttpRequest;
    xhr.onreadystatechange = function () {
        if (xhr.readyState != 4)
            return;
        if (xhr.status != 200)
            error('HTTP status: ' + xhr.status);
        else if (xhr.responseText != 'OK')
            error(xhr.responseText);
        if (callback)
            callback()
    }
    xhr.open(method, action, true);
    xhr.send(payload);
}

function removeNonFormListItems(list) {
    list.children().each(function () {
        if ($.inArray('form', this.classList))
            $(this).remove();
    });
}

function createKeyNameReloader(name, visibilityAction, callback) {
    return function () {
        $.getJSON(name, function (entries) {
            var list = $('#' + name + ' ul');
            entries = sortProperties(entries);

            $.each(entries, function (key, values) {
                var label = key == values['name'] ? key : key + ' : ' + values['name'];
                list.append('<li><h3 id="' + key + '">' + label + '</h3></li>');
                var item = list[0].lastChild;

                if (values['hidden'])
                    item.className = 'hidden';

                if (visibilityAction) {
                    $(item).append(' <a class="hide"></a>');
                    $(item.lastChild).click(function () {
                        var json = {'hide': !this.parentNode.classList.contains('hidden')}
                        json[visibilityAction] = this.parentNode.firstChild.id;

                        submitXHR('POST', 'change-visibility', JSON.stringify(json));
                        list.find('form').trigger('reload');
                    });
                }

                if (callback)
                    callback.call(item, values['hidden']);
            });
            list.append($('#' + name + ' ul .form'));
        });
    }
}

$('#branches form').bind('reload', createKeyNameReloader('branches'));
$('#platforms form').bind('reload', createKeyNameReloader('platforms', 'platform'));

$('#builders form').bind('reload', function () {
    $.getJSON('builders', function (builders) {
        var list = $('#builders ul');
        removeNonFormListItems(list);
        builders = builders.sort();
        for (var i = 0; i < builders.length; i++)
            list.append('<li><h3><a href="http://build.webkit.org/builders/' + builders[i] + '">' + builders[i] + '</a></h3></li>');
        list.append($('#builders ul .form'));
    });
});

var testReloader = createKeyNameReloader('tests', 'test', function (hidden) {
    var testName = this.firstChild.id;
    $('#tests select').append('<option value="' + testName + '">' + testName + '</option>');

});
$('#tests form').bind('reload', function () {
    var select = $('#tests select');
    select.children().remove();
    testReloader();
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

    submitXHR(this.method, this.action, payload, function () {
        $(this).trigger('reload');
    })
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
