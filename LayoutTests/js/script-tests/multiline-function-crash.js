description(
"Offset and lineNumber of the savePoint needs to restored before calling next(). Test passes if there is no crash in debug builds.");

((x = (function(){ return debug;})()
, y) => [])();
