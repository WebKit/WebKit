(function() {
    var urls = [];
    for (var i = 0; i < 1000; i++)
        urls.push("https://cdn.example.com/assets/build/" + i + "/static/media/components/very/deeply/nested/directory/structure/image-" + i + (i % 7 == 0 ? ".png" : ".webp?width=1024&quality=80"));

    var re = /\.png$/;
    var n = 400;
    var result = 0;
    for (var i = 0; i < n; i++) {
        for (var j = 0; j < urls.length; j++) {
            if (re.test(urls[j]))
                result++;
        }
    }
    if (result !== n * 143)
        throw "Error: bad result: " + result;
})();
