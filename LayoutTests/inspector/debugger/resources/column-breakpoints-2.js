function columnTest4()
{
    var x=function(){setInterval(function(){})};
    f = function()
    {
        console.log("column test 4");
//      ^ breakpoint set here (5, 8)
    }
    f();
}

function columnTest5(){var x=function(){setInterval(function(){})};(function(){console.log("column test 5")})()}
//                                                         breakpoint set here ^ (11, 79)


// Any edits of this file will necessitate a change to the breakpoint (line, column) coordinates
// used in breakpoint-columns.html.

function runColumnTest4() { setTimeout(columnTest4, 0); }
function runColumnTest5() { setTimeout(columnTest5, 0); }
