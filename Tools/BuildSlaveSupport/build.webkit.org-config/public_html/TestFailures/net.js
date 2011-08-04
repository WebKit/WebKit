var net = net || {};

(function () {

net.post = $.post;
net.get = $.get;
net.ajax = $.ajax;

net.probe = function(url, options)
{
    var scriptElement = document.createElement('script');
    scriptElement.addEventListener('load', function() {
        $(scriptElement).detach();
        if (options.success)
            options.success.call();
    }, false);
    scriptElement.addEventListener('error', function() {
        $(scriptElement).detach();
        if (options.error)
            options.error.call();
    }, false);
    scriptElement.src = url;
    document.head.appendChild(scriptElement);
};

// jQuery makes jsonp requests somewhat ugly (which is fair given that they're
// terrible for security). We use this wrapper to make our lives slightly easier.
net.jsonp = function(url, onsuccess)
{
    $.ajax({
        url: url,
        dataType: 'jsonp',
        success: onsuccess
    });
};

})();
