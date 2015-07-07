// source/credits: "Algorithm": http://www.codingforums.com/showthread.php?s=&threadid=10531
// The constructor should be called with
// the parent object (optional, defaults to window).

function Timer(){
    this.obj = (arguments.length)?arguments[0]:window;
    return this;
}

// The set functions should be called with:
// - The name of the object method (as a string) (required)
// - The millisecond delay (required)
// - Any number of extra arguments, which will all be
//   passed to the method when it is evaluated.

Timer.prototype.setInterval = function(func, msec){
    var i = Timer.getNew();
    var t = Timer.buildCall(this.obj, i, arguments);
    Timer.set[i].timer = window.setInterval(t,msec);
    return i;
}
Timer.prototype.setTimeout = function(func, msec){
    var i = Timer.getNew();
    Timer.buildCall(this.obj, i, arguments);
    Timer.set[i].timer = window.setTimeout("Timer.callOnce("+i+");",msec);
    return i;
}

// The clear functions should be called with
// the return value from the equivalent set function.

Timer.prototype.clearInterval = function(i){
    if(!Timer.set[i]) return;
    window.clearInterval(Timer.set[i].timer);
    Timer.set[i] = null;
}
Timer.prototype.clearTimeout = function(i){
    if(!Timer.set[i]) return;
    window.clearTimeout(Timer.set[i].timer);
    Timer.set[i] = null;
}

// Private data

Timer.set = new Array();
Timer.buildCall = function(obj, i, args){
    var t = "";
    Timer.set[i] = new Array();
    if(obj != window){
        Timer.set[i].obj = obj;
        t = "Timer.set["+i+"].obj.";
    }
    t += args[0]+"(";
    if(args.length > 2){
        Timer.set[i][0] = args[2];
        t += "Timer.set["+i+"][0]";
        for(var j=1; (j+2)<args.length; j++){
            Timer.set[i][j] = args[j+2];
            t += ", Timer.set["+i+"]["+j+"]";
    }}
    t += ");";
    Timer.set[i].call = t;
    return t;
}
Timer.callOnce = function(i){
    if(!Timer.set[i]) return;
    eval(Timer.set[i].call);
    Timer.set[i] = null;
}
Timer.getNew = function(){
    var i = 0;
    while(Timer.set[i]) i++;
    return i;
}