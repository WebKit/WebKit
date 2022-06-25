var kotoeriState = { "compositionString": null };
function kotoeri(event){
    if (event.characters[0] == '\n') {
        if (kotoeriState.compositionString == null) {
            textInputController.doCommand("insertNewline:");
            return true;
        }
        textInputController.insertText(kotoeriState.compositionString);
        kotoeriState.compositionString = null;
        return true;
    }        
        
    if (kotoeriState.compositionString == null) 
         kotoeriState.compositionString = "";
    kotoeriState.compositionString += event.characters;
    var markedText = textInputController.makeAttributedString(kotoeriState.compositionString);
    markedText.addAttribute("NSUnderline", 2);
    textInputController.setMarkedText(markedText, markedText.length, 0);
    return true;
}
