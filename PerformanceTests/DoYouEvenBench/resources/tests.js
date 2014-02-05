var numberOfItemsToAdd = 100;
var Suites = [];

Suites.push({
    name: 'VanillaJS-TodoMVC',
    url: 'todomvc/vanilla-examples/vanillajs/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            var todoController = contentWindow.todo.controller;
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                todoController.addItem({keyCode: todoController.ENTER_KEY, target: newTodo});
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'EmberJS-TodoMVC',
    url: 'todomvc/architecture-examples/emberjs/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return {
                views: contentWindow.Ember.View.views,
                emberRun: contentWindow.Ember.run,
            }
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (params) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                params.emberRun(function () { params.views["new-todo"].set('value', 'Something to do' + i); });
                params.emberRun(function () { params.views["new-todo"].insertNewline(document.createEvent('Event')); });
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.ember-checkbox');
            for (var i = 0; i < checkboxes.length; i++) {
                var view = params.views[checkboxes[i].id];
                params.emberRun(function () { view.set('checked', true); });
            }
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                params.emberRun(function () { deleteButtons[i].click(); });
        }),
    ]
});

Suites.push({
    name: 'BackboneJS-TodoMVC',
    url: 'todomvc/architecture-examples/backbone/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
    contentWindow.Backbone.sync = function () {}
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            var appView = contentWindow.appView;
            var fakeEvent = {which: contentWindow.ENTER_KEY};
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                appView.createOnEnter(fakeEvent);
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'jQuery-TodoMVC',
    url: 'todomvc/architecture-examples/jquery/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            var app = contentWindow.app;
            var fakeEvent = {which: app.ENTER_KEY};
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                app.create.call(newTodo, fakeEvent);
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var app = contentWindow.app;
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            var $ = contentWindow.$;

            itemIndexToId = new Array(checkboxes.length);
            for (var i = 0; i < checkboxes.length; i++)
                itemIndexToId[i] = $(checkboxes[i]).closest('li').data('id');

            app.getTodo = function (element, callback) {
                var self = this;
                var id = itemIndexToId[this.currentItemIndex];
                $.each(this.todos, function (j, val) {
                    if (val.id === id) {
                        callback.apply(self, arguments);
                        return false;
                    }
                });
            }

            for (var i = 0; i < checkboxes.length; i++) {
                app.currentItemIndex = i;
                app.toggle.call(checkboxes[i]);
            }
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            contentDocument.querySelector('#clear-completed').click();
            var app = contentWindow.app;
            var deleteButtons = contentDocument.querySelectorAll('.destroy');

            for (var i = 0; i < deleteButtons.length; i++) {
                app.currentItemIndex = i;
                app.destroy.call(deleteButtons[i]);
            }
        }),
    ]
});

Suites.push({
    name: 'AngularJS-TodoMVC',
    url: 'todomvc/architecture-examples/angularjs/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            var todomvc = contentWindow.todomvc;
            var submitEvent = document.createEvent('Event');
            submitEvent.initEvent('submit', true, true);
            var inputEvent = document.createEvent('Event');
            inputEvent.initEvent('input', true, true);
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(inputEvent);
                newTodo.form.dispatchEvent(submitEvent);
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'React-TodoMVC',
    url: 'todomvc/labs/architecture-examples/react/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        contentWindow.Utils.store = function () {}
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            var todomvc = contentWindow.todomvc;
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;

                var keydownEvent = document.createEvent('Event');
                keydownEvent.initEvent('keydown', true, true);
                keydownEvent.which = 13; // VK_ENTER
                newTodo.dispatchEvent(keydownEvent);
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

var actionCount = 50;
Suites.push({
    name: 'FlightJS-MailClient',
    url: 'flightjs-example-app/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.span8').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('OpeningTabs' + actionCount + 'Times', function (newTodo, contentWindow, contentDocument) {
            contentDocument.getElementById('inbox').click();
            for (var i = 0; i < actionCount; i++) {
                contentDocument.getElementById('later').click();
                contentDocument.getElementById('sent').click();
                contentDocument.getElementById('trash').click();
                contentDocument.getElementById('inbox').click();
            }
        }),
        new BenchmarkTestStep('MovingEmails' + actionCount + 'Times', function (newTodo, contentWindow, contentDocument) {
            contentDocument.getElementById('inbox').click();
            for (var i = 0; i < actionCount; i++) {
                contentDocument.getElementById('mail_2139').click();
                contentDocument.getElementById('move_mail').click();
                contentDocument.querySelector('#move_to_selector #later').click();
                contentDocument.getElementById('later').click();
                contentDocument.getElementById('mail_2139').click();
                contentDocument.getElementById('move_mail').click();
                contentDocument.querySelector('#move_to_selector #trash').click();
                contentDocument.getElementById('trash').click();
                contentDocument.getElementById('mail_2139').click();
                contentDocument.getElementById('move_mail').click();
                contentDocument.querySelector('#move_to_selector #inbox').click();
                contentDocument.getElementById('inbox').click();
            }
        }),
        new BenchmarkTestStep('Sending' + actionCount + 'NewEmails', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < actionCount; i++) {
                contentDocument.getElementById('new_mail').click();
                contentDocument.getElementById('recipient_select').selectedIndex = 1;
                var subject = contentDocument.getElementById('compose_subject');
                var message = contentDocument.getElementById('compose_message');
                subject.focus();
                contentWindow.$(subject).trigger('keydown');
                contentWindow.$(subject).text('Hello');
                message.focus();
                contentWindow.$(message).trigger('keydown');
                contentWindow.$(message).text('Hello,\n\nThis is a test message.\n\n- WebKitten');
                contentDocument.getElementById('send_composed').click();
            }
        }),
    ]
});
