var myArray = Array();
myArray[ 10000 ] = "a";
myArray[ 10001 ] = "b";
myArray[ 10002 ] = "c";

// remove element at index 1001
myArray.splice( 10001, 1 );

if (myArray[10000] != "a")
    throw "Splicing Error! start index changed";
if (myArray[10001] != "c")
    throw "Splicing Error! removed element not removed";
