description('This test covers the correctness and behaviour of switch statements.');

// To make sure that there are no jump table indexing problems each of these functions
// has multiple no-op switch statements to force not trivial jump table offsets for the
// primary switch being tested.
function characterSwitch(scrutinee) {
    switch(null){
        case '_':
    }
    switch(scrutinee) {
        case '\0':
            return '\0';
        case 'A':
            return 'A';
        case 'a':
            return 'a';
        case 'A':
            return 'Should not hit second \'A\'';
        case '1':
            return '1';
        default:
            return 'default';
        case 'B':
            return 'B';
            switch(null){
                case '1':
            }
    }
    switch(null){
        case '_':
    }
    return 'Did not reach default clause';
}

function sparseCharacterSwitch(scrutinee) {
    switch(null){
        case '1':
    }
    switch(null){
        case '12':
    }
    switch(scrutinee) {
        case '\0':
            return '\0';
        case 'A':
            return 'A';
        case 'a':
            return 'a';
        case 'A':
            return 'Should not hit second \'A\'';
        case '1':
            return '1';
        default:
            return 'default';
        case 'B':
            return 'B';
        case '\uffff':
            return '\uffff';
            switch(null){
                case '1':
            }
            switch(null){
                case '12':
            }
    }
    return 'Did not reach default clause';
    switch(null){
        case '1':
    }
    switch(null){
        case '12':
    }
}

function stringSwitch(scrutinee) {
    switch(null){
        case '12':
    }
    switch(scrutinee) {
        case '\0':
            return '\0';
        case 'A':
            return 'A';
        case 'a':
            return 'a';
        case 'A':
            return 'Should not hit second \'A\'';
        case '1':
            return '1';
        case '-1':
            return '-1';
        case '[object Object]':
            return '[object Object]';
        case 'some string':
            return 'some string';
        default:
            return 'default';
        case 'B':
            return 'B';
        case '\uffff':
            return '\uffff'
            switch(null){
                case '12':
            }
    }
    return 'Did not reach default clause';
    switch(null){
        case '12':
    }
}

function numberSwitch(scrutinee) {
    switch(null){
        case 1:
    }
    switch(scrutinee) {
        case  0:
            return 0;
        case  1:
            return 1;
        case  1:
            return 'Should not hit second 1';
        default:
            return 'default';
        case -1:
            return -1;
        switch(null){
            case 1:
        }
    }
    return 'Did not reach default clause';
    switch(null){
        case 1:
    }
}

function sparseNumberSwitch(scrutinee) {
    switch(null){
        case 1:
    }
    switch(scrutinee) {
        case  0:
            return 0;
        case  1:
            return 1;
        case  1:
            return 'Should not hit second 1';
        default:
            return 'default';
        case -1:
            return -1;
        case -1000000000:
            return -1000000000;
        case 1000000000:
            return 1000000000;
        switch(null){
            case 1:
        }
    }
    return 'Did not reach default clause';
    switch(null){
        case 1:
    }
}

function generalSwitch(scrutinee) {
    switch(null){
        case 1:
    }
    switch(null){
        case '1':
    }
    switch(null){
        case '12':
    }
    switch(scrutinee) {
        case  0:
            return 0;
        case  1:
            return 1;
        case  1:
            return 'Should not hit second 1';
        default:
            return 'default';
        case -1:
            return -1;
        case -1000000000:
            return -1000000000;
        case 1000000000:
            return 1000000000;
        case '\0':
            return '\0';
        case 'A':
            return 'A';
        case 'a':
            return 'a';
        case 'A':
            return 'Should not hit second \'A\'';
        case '1':
            return '1';
        case '-1':
            return '-1';
        case '[object Object]':
            return '[object Object]';
        case 'some string':
            return 'some string';
        case 'B':
            return 'B';
        case '\uffff':
            return '\uffff'
            switch(null){
                case 1:
            }
            switch(null){
                case '1':
            }
            switch(null){
                case '12':
            }
    }
    return 'Did not reach default clause';
    switch(null){
        case 1:
    }
    switch(null){
        case '1':
    }
    switch(null){
        case '12':
    }
}

// Character switch
var emptyString1 = "";
var emptyString2 = "";
shouldBe("characterSwitch('A' + emptyString1)", '"A"');
shouldBe("characterSwitch('A' + emptyString1 + emptyString2)", '"A"');
shouldBe("characterSwitch(emptyString1 + emptyString2)", '"default"');

shouldBe("characterSwitch('\0')", '"\0"');
shouldBe("characterSwitch('A')", '"A"');
shouldBe("characterSwitch('a')", '"a"');
shouldBe("characterSwitch('1')", '"1"');
shouldBe("characterSwitch('-1')", '"default"');
shouldBe("characterSwitch('B')", '"B"');
shouldBe("characterSwitch('\uffff')", '"default"');
shouldBe("characterSwitch({toString: function(){return 'B'}})", '"default"');
shouldBe("characterSwitch(0)", '"default"');
shouldBe("characterSwitch(-0)", '"default"');
shouldBe("characterSwitch(1)", '"default"');
shouldBe("characterSwitch(1.1)", '"default"');
shouldBe("characterSwitch(-1)", '"default"');
shouldBe("characterSwitch(-1000000000)", '"default"');
shouldBe("characterSwitch(1000000000)", '"default"');
shouldBe("characterSwitch({})", '"default"');

// Sparse character switch
shouldBe("sparseCharacterSwitch('\0')", '"\0"');
shouldBe("sparseCharacterSwitch('A')", '"A"');
shouldBe("sparseCharacterSwitch('a')", '"a"');
shouldBe("sparseCharacterSwitch('1')", '"1"');
shouldBe("sparseCharacterSwitch('-1')", '"default"');
shouldBe("sparseCharacterSwitch('B')", '"B"');
shouldBe("sparseCharacterSwitch('\uffff')", '"\uffff"');
shouldBe("sparseCharacterSwitch({toString: function(){return 'B'}})", '"default"');
shouldBe("sparseCharacterSwitch(0)", '"default"');
shouldBe("sparseCharacterSwitch(-0)", '"default"');
shouldBe("sparseCharacterSwitch(1)", '"default"');
shouldBe("sparseCharacterSwitch(1.1)", '"default"');
shouldBe("sparseCharacterSwitch(-1)", '"default"');
shouldBe("sparseCharacterSwitch(-1000000000)", '"default"');
shouldBe("sparseCharacterSwitch(1000000000)", '"default"');
shouldBe("sparseCharacterSwitch({})", '"default"');

// String switch
shouldBe("stringSwitch('\0')", '"\0"');
shouldBe("stringSwitch('A')", '"A"');
shouldBe("stringSwitch('a')", '"a"');
shouldBe("stringSwitch('1')", '"1"');
shouldBe("stringSwitch('-1')", '"-1"');
shouldBe("stringSwitch('B')", '"B"');
shouldBe("stringSwitch('\uffff')", '"\uffff"');
shouldBe("stringSwitch('some string')", '"some string"');
shouldBe("stringSwitch({toString: function(){return 'some string'}})", '"default"');
shouldBe("stringSwitch('s')", '"default"');
shouldBe("stringSwitch(0)", '"default"');
shouldBe("stringSwitch(-0)", '"default"');
shouldBe("stringSwitch(1)", '"default"');
shouldBe("stringSwitch(1.1)", '"default"');
shouldBe("stringSwitch(-1)", '"default"');
shouldBe("stringSwitch(-1000000000)", '"default"');
shouldBe("stringSwitch(1000000000)", '"default"');
shouldBe("stringSwitch({})", '"default"');

// Number switch
shouldBe("numberSwitch('\0')", '"default"');
shouldBe("numberSwitch('A')", '"default"');
shouldBe("numberSwitch('a')", '"default"');
shouldBe("numberSwitch('1')", '"default"');
shouldBe("numberSwitch('-1')", '"default"');
shouldBe("numberSwitch('B')", '"default"');
shouldBe("numberSwitch('\uffff')", '"default"');
shouldBe("numberSwitch('some string')", '"default"');
shouldBe("numberSwitch({valueOf: function(){return 0}})", '"default"');
shouldBe("numberSwitch('s')", '"default"');
shouldBe("numberSwitch(0)", '0');
shouldBe("numberSwitch(-0)", '0');
shouldBe("numberSwitch(1)", '1');
shouldBe("numberSwitch(1.1)", '"default"');
shouldBe("numberSwitch(-1)", '-1');
shouldBe("numberSwitch(-1000000000)", '"default"');
shouldBe("numberSwitch(1000000000)", '"default"');
shouldBe("numberSwitch({})", '"default"');

// Sparse number switch
shouldBe("sparseNumberSwitch('\0')", '"default"');
shouldBe("sparseNumberSwitch('A')", '"default"');
shouldBe("sparseNumberSwitch('a')", '"default"');
shouldBe("sparseNumberSwitch('1')", '"default"');
shouldBe("sparseNumberSwitch('-1')", '"default"');
shouldBe("sparseNumberSwitch('B')", '"default"');
shouldBe("sparseNumberSwitch('\uffff')", '"default"');
shouldBe("sparseNumberSwitch('some string')", '"default"');
shouldBe("sparseNumberSwitch({valueOf: function(){return 0}})", '"default"');
shouldBe("sparseNumberSwitch('s')", '"default"');
shouldBe("sparseNumberSwitch(0)", '0');
shouldBe("sparseNumberSwitch(-0)", '0');
shouldBe("sparseNumberSwitch(1)", '1');
shouldBe("sparseNumberSwitch(1.1)", '"default"');
shouldBe("sparseNumberSwitch(-1)", '-1');
shouldBe("sparseNumberSwitch(-1000000000)", '-1000000000');
shouldBe("sparseNumberSwitch(1000000000)", '1000000000');
shouldBe("sparseNumberSwitch({})", '"default"');

// General switch
shouldBe("generalSwitch('\0')", '"\0"');
shouldBe("generalSwitch('A')", '"A"');
shouldBe("generalSwitch('a')", '"a"');
shouldBe("generalSwitch('1')", '"1"');
shouldBe("generalSwitch('-1')", '"-1"');
shouldBe("generalSwitch('B')", '"B"');
shouldBe("generalSwitch('\uffff')", '"\uffff"');
shouldBe("generalSwitch('some string')", '"some string"');
shouldBe("generalSwitch({valueOf: function(){return 0}})", '"default"');
shouldBe("generalSwitch('s')", '"default"');
shouldBe("generalSwitch(0)", '0');
shouldBe("generalSwitch(-0)", '0');
shouldBe("generalSwitch(1)", '1');
shouldBe("generalSwitch(1.1)", '"default"');
shouldBe("generalSwitch(-1)", '-1');
shouldBe("generalSwitch(-1000000000)", '-1000000000');
shouldBe("generalSwitch(1000000000)", '1000000000');
shouldBe("generalSwitch({})", '"default"');
