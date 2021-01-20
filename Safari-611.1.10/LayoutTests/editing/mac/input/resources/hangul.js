/*
 * Pseudo hangul-IM, only one character is marked at a time, 
 * enter confirms the composition, but is passed on to the app to handle as a keypress
 */
var hangulState = { "compositionString": null };
function hangul(event){
    if (event.characters[0] == '\n') {
        if (hangulState.compositionString != null) 
            textInputController.insertText(hangulState.compositionString);
        hangulState.compositionString  = null;
        textInputController.doCommand("insertNewline:");
        return true;
    }        
        
    if (hangulState.compositionString != null)
        textInputController.insertText(hangulState.compositionString);
        
    hangulState.compositionString = event.characters;
    var markedText = textInputController.makeAttributedString(hangulState.compositionString);
    markedText.addAttribute("NSUnderline", 1);
    textInputController.setMarkedText(markedText, markedText.length, 0);
    return true;
}
