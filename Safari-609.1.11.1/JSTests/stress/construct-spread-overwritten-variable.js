//@ runDefault

(function(){
    var x = 42;
    var a = [1, 2, 3];
    new x(x = function(){ }, ...a);
})();
