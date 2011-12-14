description('&lt;select&gt; selection test for mouse events and keyevents.');

function mouseDownOnSelect(selId, index, modifier) {
    var sl = document.getElementById(selId);
    var itemHeight = Math.floor(sl.offsetHeight / sl.size);
    var border = 1;
    var y = border + index * itemHeight;

    sl.focus();
    if (window.eventSender) {
        eventSender.mouseMoveTo(sl.offsetLeft + border, sl.offsetTop + y - window.pageYOffset);
        eventSender.mouseDown(0, [modifier]);
        eventSender.mouseUp(0, [modifier]);
    }
}

function keyDownOnSelect(selId, identifier, modifier) {
    document.getElementById(selId).focus();
    if (window.eventSender)
        eventSender.keyDown(identifier, [modifier]);
}

function createSelect(idName, sz, mlt, selIndex) {
    var sl = document.createElement("select");
    var i = 0;
    sl.size = sz;
    while (i < sz) {
        var opt = document.createElement("option");
        if (i == selIndex)
            opt.selected = true;
        opt.textContent = "item " + i;
        sl.appendChild(opt);
        i++;
    }
    sl.multiple = mlt;
    sl.id = idName;
    var parent = document.getElementById("parent");
    parent.appendChild(sl);
}

function selectionPattern(selId) {
    var sl = document.getElementById(selId);
    var result = '';
    for (var i = 0; i < sl.options.length; i++)
        result += sl.options[i].selected ? '1' : '0';
    return result;
}

var parent = document.createElement('div');
parent.id = "parent";
document.body.appendChild(parent);

createSelect("sl1", 5, false, -1);
createSelect("sl2", 5, false, 1);
createSelect("sl3", 5, false, -1);
createSelect("sl4", 5, false, 1);
createSelect("sl5", 5, false, 2);
createSelect("sl6", 5, false, 3);
createSelect("sl7", 5, false, 1);

createSelect("sl8", 5, true, -1);
createSelect("sl9", 5, true, 1);
createSelect("sl10", 5, true, -1);
createSelect("sl11", 5, true, 1);
createSelect("sl12", 5, true, 2);
createSelect("sl13", 5, true, 0);
createSelect("sl14", 5, true, 1);

debug("1) Select one item with mouse (no previous selection)");
mouseDownOnSelect("sl1", 0);
shouldBe('selectionPattern("sl1")', '"10000"');

debug("2) Select one item with mouse (with previous selection)");
mouseDownOnSelect("sl2", 0);
shouldBe('selectionPattern("sl2")', '"10000"');

debug("3) Select one item with the keyboard (no previous selection)");
keyDownOnSelect("sl3", "upArrow");
shouldBe('selectionPattern("sl3")', '"00001"');

debug("4) Select one item with the keyboard (with previous selection)");
keyDownOnSelect("sl4", "downArrow");
shouldBe('selectionPattern("sl4")', '"00100"');

debug("5) Attempt to select an item cmd-clicking");
mouseDownOnSelect("sl5", 1, "addSelectionKey");
shouldBe('selectionPattern("sl5")', '"01000"');

debug("6) Attempt to select a range shift-clicking");
mouseDownOnSelect("sl6", 1, "rangeSelectionKey");
shouldBe('selectionPattern("sl6")', '"01000"');

debug("7) Attempt to select a range with the keyboard");
keyDownOnSelect("sl7", "downArrow", "rangeSelectionKey");
shouldBe('selectionPattern("sl7")', '"00100"');

// Multiple selection tests

debug("8) Select one item with mouse (no previous selection)");
mouseDownOnSelect("sl8", 0);
shouldBe('selectionPattern("sl8")', '"10000"');

debug("9) Select one item with mouse (with previous selection)");
mouseDownOnSelect("sl9", 0);
shouldBe('selectionPattern("sl9")', '"10000"');

debug("10) Select one item with the keyboard (no previous selection)");
keyDownOnSelect("sl10", "upArrow");
shouldBe('selectionPattern("sl10")', '"00001"');

debug("11) Select one item with the keyboard (with previous selection)");
keyDownOnSelect("sl11", "downArrow");
shouldBe('selectionPattern("sl11")', '"00100"');

debug("12) Select an item cmd-clicking");
mouseDownOnSelect("sl12", 1, "addSelectionKey");
shouldBe('selectionPattern("sl12")', '"01100"');

debug("13) Select a range shift-clicking");
mouseDownOnSelect("sl13", 3, "rangeSelectionKey");
shouldBe('selectionPattern("sl13")', '"11110"');

debug("14) Select a range with the keyboard");
keyDownOnSelect("sl14", "downArrow", "rangeSelectionKey");
shouldBe('selectionPattern("sl14")', '"01100"');

var successfullyParsed = true;
