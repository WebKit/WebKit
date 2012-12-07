description(
"Tests if the CFG simplifier gracefully handles the case where Block #1 and #2 are merged, #1 has a dead GetLocal, and #2 has a live GetLocal on the same local."
);

var array = [];

var getDist = function () {
    // a conditional absolutely MUST be here for the whole thing to break.
    // the return value is irrelevant

    if (false) return "I'm irrelevant!";

    return Math.sqrt(5);
}


var calcError = function(){
    var dist = 0; // initialisation is necessary for the bug to occur

    true && (dist = getDist());

    array.push(dist);
}


for(var a=0; a<1000; a++){ // the loop needs to be outside calcError, otherwise no bug...
    calcError();
}

for (var i = 0; i < array.length; ++i)
    shouldBe("array[i]", "2.23606797749979");
