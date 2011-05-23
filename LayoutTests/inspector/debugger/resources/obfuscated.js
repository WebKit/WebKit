function formatted1()
{
    var variable1 = 0;
}

function withComments()
{
// comment
    return "functionWithComments";
}

try{onmessage=function(event){var source=event.data;var formattedSource=beautify(source);var mapping=buildMapping(source,formattedSource);postMessage({formattedSource:formattedSource,mapping:mapping})};function beautify(source){var ast=parse.parse(source);var beautifyOptions=
{indent_level:4,indent_start:0,quote_keys:false,space_colon:false};return process.gen_code(ast,beautifyOptions)}function buildMapping(source,formattedSource){var mapping={original:[],formatted:[]};var lastPosition=0;var regexp=/(^|[^\\])\b((?=\D)[\$\.\w]+)\b/g;while(true)
{var match=regexp.exec(formattedSource);if(!match)break;var position=source.indexOf(match[2],lastPosition);if(position===-1)throw"No match found in original source for "+match[2];mapping.original.push(position);mapping.formatted.push(match.index+match[1].length);
lastPosition=position+match[2].length}return mapping}function require(){return parse}var exports={};importScripts("UglifyJS/parse-js.js");var parse=exports;var exports={};importScripts("UglifyJS/process.js");var process=exports;}catch(e){}

function formatted2()
{
    var variable2 = 0;
}
