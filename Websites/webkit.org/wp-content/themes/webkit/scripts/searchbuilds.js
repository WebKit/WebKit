(function(document) {

    var inputList = document.querySelectorAll('input.buildsearch'),
        ui = document.getElementById('search-results'),
        spinner = document.getElementById('search-spinner'),
        errors = document.getElementById('search-errors'),
        loaded = ui.lastChild.cloneNode(true),
        inputs = [].slice.call(inputList);

    for (var i in inputs) {
        var input = inputs[i];
        initsearch(input);
    }

    function initsearch(input) {

        var delayTimer = null;

        // input.addEventListener('focus', sendSearch);
        input.addEventListener('keyup', function () {
            if ( delayTimer != null )
                clearTimeout(delayTimer);
            delayTimer = setTimeout(sendSearch, 500);
        });

        function xhrPromise(url) {
            return new Promise(function(resolve, reject) {
                var XHR = new XMLHttpRequest();
                XHR.open('GET', url, true);
                XHR.responseType = "json";

                XHR.onload = function() {
                    if (XHR.status == 200 && XHR.response) {
                        resolve(XHR.response);
                    } else reject({request: XHR, url:url});
                };
                XHR.onerror = function() {
                    reject({request: XHR, url:url});
                };
                XHR.send();
            });
        }

        // Send the search query
        function sendSearch (e) {
            delayTimer = null;
            clearErrors();
            var query = input.value.toLowerCase();
            if (query == '' || query.length < 3)
                return ui.replaceChild(loaded, ui.lastChild);

            var endpoint = new URL(ajaxurl + "?action=search_nightly_builds&query=" + query);
            var searchResults = xhrPromise(endpoint);
            spinner.classList.add('searching');
            searchResults.then(displayResults).catch(displayError);
        }

        function displayResults(results) {
            var list = document.createElement('ul');
            if ( results.length == 0 )
                return displayError('There are no builds matching your search. Search by revision number, or a range of revision numbers: ######-######');

            for (var entry of results)
                addEntry(entry[0], entry[1], entry[2]);
            ui.replaceChild(list, ui.lastChild);
            spinner.classList.remove('searching');

            function addEntry (build, datetime, download) {
                var li = document.createElement('li'),
                    link = document.createElement('a'),
                date = document.createElement('span');

                link.href = download;
                link.innerText = "r" + build;

                date.classList.add('date');
                date.innerText = datetime;

                li.appendChild(link);
                li.appendChild(date);
                list.appendChild(li);
            }
        }

        function displayError(message) {
            var note = document.createElement('div');

            if ( typeof message !== 'string' )
                message = 'A communication error occured preventing any results from being returned by the server.';

            note.classList.add('note');
            note.innerHTML = message;

            errors.appendChild(note);

            spinner.classList.remove('searching');
        }

        function clearErrors() {
            while (errors.firstChild)
                errors.removeChild(errors.firstChild);
        }

     }

}(document));
