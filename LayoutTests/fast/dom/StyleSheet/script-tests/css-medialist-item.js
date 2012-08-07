description("This tests indexing outside the range of the media list object.");

var styleElement  = document.createElement('style');
styleElement.setAttribute('media', 'screen, print');
document.documentElement.appendChild(styleElement)
var mediaList = document.styleSheets[document.styleSheets.length - 1].media;

shouldEvaluateTo('mediaList.length', 2);
shouldBeEqualToString('mediaList[0]', 'screen');
shouldBeEqualToString('mediaList[1]', 'print');
shouldBeUndefined('mediaList[2]');
shouldBeUndefined('mediaList[-1]')

document.documentElement.removeChild(styleElement);
