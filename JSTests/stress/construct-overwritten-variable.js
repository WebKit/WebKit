//@ runDefault

(function(){
    var x = 42;
    new x(x = function(){ });
})();
