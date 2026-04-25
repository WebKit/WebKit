function columnTest1(){var x=function(){setInterval(function(){})};(function(){console.log("column test 1")})()}
//                                                         breakpoint set here ^ (0, 79)

function columnTest2()
{
    var x=function(){setInterval(function(){})};
    f = function() { console.log("column test 2"); }();
//                   ^ breakpoint set here (6, 21)
}

function columnTest3()
{
    var x=function(){setInterval(function(){})};
    f = function()
    {
        console.log("column test 3");
//      ^ breakpoint set here (15, 8)
    }
    f();
}

// Any edits of this file will necessitate a change to the breakpoint (line, column) coordinates
// used in breakpoint-columns.html.

function runColumnTest1() { setTimeout(columnTest1, 0); }
function runColumnTest2() { setTimeout(columnTest2, 0); }
function runColumnTest3() { setTimeout(columnTest3, 0); }
