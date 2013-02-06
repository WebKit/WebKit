function changeDisplayIfNeeded(element) {
    if(element.getAttribute) {
        var additionalClasses = '';
        if(element.getAttribute('makeInline')) {
            if (element.tagName == 'TABLE') 
                additionalClasses += ' inline-table';
            else
                additionalClasses += ' inline';
        }
        else if (element.getAttribute('makeBlock')) {
            if (element.tagName == 'TABLE') 
                additionalClasses += ' table';
            else
                additionalClasses += ' block';
        }
        element.className += additionalClasses;
    }

}

function runTest(additionalClass) {
    var testElems = document.getElementsByClassName('testElement');
    for(var i = 0; i < testElems.length; i++) {
        var element = testElems[i];
        changeDisplayIfNeeded(element);
        element.className += ' ' + additionalClass.substring(1);
        changeDisplayIfNeeded(element.previousSibling);  
    }
    checkLayout('.testElement');
}
