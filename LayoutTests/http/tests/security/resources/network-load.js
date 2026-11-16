(function (global_scope)
{
    function promise_test(func, name, properties) {
        var test = new Test(name, properties);
        tests.promise_tests = Promise.resolve();
        tests.promise_tests = tests.promise_tests.then(function() {
            return new Promise(function(resolve) {
                var promise = test.step(func, test, test);
                Promise.resolve(promise)
                });
        });
    }
   
    expose(promise_test, 'trigger');

    function Test(name, properties)
    {
        this.name = name;
        tests.push(this);
    }


    Test.prototype.step = function(func, this_obj)
    {
        return func.apply(this_obj, Array.prototype.slice.call(arguments, 2));  
    };

    Test.prototype.step_func = function(func, this_obj)
    {
        var test_this = this;

        return function()
        {
            return test_this.step.apply(test_this, [func, this_obj].concat(
                Array.prototype.slice.call(arguments)));
        };
    };

    function Tests() {}
    Tests.prototype.push = function(test) {}

    function expose(object, name)
    {
        var components = name.split(".");
        var target = global_scope;
        for (var i = 0; i < components.length - 1; i++) {
            target = target[components[i]];
        }
        target[components[components.length - 1]] = object;
    }
    var tests = new Tests();
})(self);
