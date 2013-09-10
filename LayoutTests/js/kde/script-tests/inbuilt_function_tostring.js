   function StripSpaces( s ) {
        for ( var currentChar = 0, strippedString="";
                currentChar < s.length; currentChar++ )
        {
            if (!IsWhiteSpace(s.charAt(currentChar))) {
                strippedString += s.charAt(currentChar);
            }
        }
        return strippedString;
    }

    function IsWhiteSpace( string ) {
        var cc = string.charCodeAt(0);
        switch (cc) {
            case (0x0009):
            case (0x000B):
            case (0x000C):
            case (0x0020):
            case (0x000A):
            case (0x000D):
            case ( 59 ): // let's strip out semicolons, too
                return true;
                break;
            default:
                return false;
        }
    }


shouldBe("StripSpaces(eval.toString())","\"functioneval(){[nativecode]}\"");
shouldBe("StripSpaces(parseInt.toString())","\"functionparseInt(){[nativecode]}\"");
shouldBe("StripSpaces(parseFloat.toString())","\"functionparseFloat(){[nativecode]}\"");
shouldBe("StripSpaces(isNaN.toString())","\"functionisNaN(){[nativecode]}\"");
shouldBe("StripSpaces(isFinite.toString())","\"functionisFinite(){[nativecode]}\"");
shouldBe("StripSpaces(escape.toString())","\"functionescape(){[nativecode]}\"");
shouldBe("StripSpaces(unescape.toString())","\"functionunescape(){[nativecode]}\"");

shouldBe("StripSpaces(Object.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(Object.prototype.toLocaleString.toString())","\"functiontoLocaleString(){[nativecode]}\"");
shouldBe("StripSpaces(Object.prototype.valueOf.toString())","\"functionvalueOf(){[nativecode]}\"");
shouldBe("StripSpaces(Object.prototype.hasOwnProperty.toString())","\"functionhasOwnProperty(){[nativecode]}\"");
shouldBe("StripSpaces(Object.prototype.isPrototypeOf.toString())","\"functionisPrototypeOf(){[nativecode]}\"");
shouldBe("StripSpaces(Object.prototype.propertyIsEnumerable.toString())","\"functionpropertyIsEnumerable(){[nativecode]}\"");

shouldBe("StripSpaces(Function.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(Function.prototype.apply.toString())","\"functionapply(){[nativecode]}\"");
shouldBe("StripSpaces(Function.prototype.call.toString())","\"functioncall(){[nativecode]}\"");

shouldBe("StripSpaces(Array.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.toLocaleString.toString())","\"functiontoLocaleString(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.concat.toString())","\"functionconcat(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.join.toString())","\"functionjoin(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.pop.toString())","\"functionpop(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.push.toString())","\"functionpush(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.reverse.toString())","\"functionreverse(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.shift.toString())","\"functionshift(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.slice.toString())","\"functionslice(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.sort.toString())","\"functionsort(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.splice.toString())","\"functionsplice(){[nativecode]}\"");
shouldBe("StripSpaces(Array.prototype.unshift.toString())","\"functionunshift(){[nativecode]}\"");

shouldBe("StripSpaces(String.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.valueOf.toString())","\"functionvalueOf(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.charAt.toString())","\"functioncharAt(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.charCodeAt.toString())","\"functioncharCodeAt(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.concat.toString())","\"functionconcat(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.indexOf.toString())","\"functionindexOf(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.lastIndexOf.toString())","\"functionlastIndexOf(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.match.toString())","\"functionmatch(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.replace.toString())","\"functionreplace(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.search.toString())","\"functionsearch(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.slice.toString())","\"functionslice(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.split.toString())","\"functionsplit(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.substr.toString())","\"functionsubstr(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.substring.toString())","\"functionsubstring(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.toLowerCase.toString())","\"functiontoLowerCase(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.toUpperCase.toString())","\"functiontoUpperCase(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.big.toString())","\"functionbig(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.small.toString())","\"functionsmall(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.blink.toString())","\"functionblink(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.bold.toString())","\"functionbold(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.fixed.toString())","\"functionfixed(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.italics.toString())","\"functionitalics(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.strike.toString())","\"functionstrike(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.sub.toString())","\"functionsub(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.sup.toString())","\"functionsup(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.fontcolor.toString())","\"functionfontcolor(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.fontsize.toString())","\"functionfontsize(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.anchor.toString())","\"functionanchor(){[nativecode]}\"");
shouldBe("StripSpaces(String.prototype.link.toString())","\"functionlink(){[nativecode]}\"");

shouldBe("StripSpaces(Boolean.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(Boolean.prototype.valueOf.toString())","\"functionvalueOf(){[nativecode]}\"");

shouldBe("StripSpaces(Number.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(Number.prototype.toLocaleString.toString())","\"functiontoLocaleString(){[nativecode]}\"");
shouldBe("StripSpaces(Number.prototype.valueOf.toString())","\"functionvalueOf(){[nativecode]}\"");
shouldBe("StripSpaces(Number.prototype.toFixed.toString())","\"functiontoFixed(){[nativecode]}\"");
shouldBe("StripSpaces(Number.prototype.toExponential.toString())","\"functiontoExponential(){[nativecode]}\"");
shouldBe("StripSpaces(Number.prototype.toPrecision.toString())","\"functiontoPrecision(){[nativecode]}\"");

shouldBe("StripSpaces(Math.abs.toString())","\"functionabs(){[nativecode]}\"");
shouldBe("StripSpaces(Math.acos.toString())","\"functionacos(){[nativecode]}\"");
shouldBe("StripSpaces(Math.asin.toString())","\"functionasin(){[nativecode]}\"");
shouldBe("StripSpaces(Math.atan.toString())","\"functionatan(){[nativecode]}\"");
shouldBe("StripSpaces(Math.atan2.toString())","\"functionatan2(){[nativecode]}\"");
shouldBe("StripSpaces(Math.ceil.toString())","\"functionceil(){[nativecode]}\"");
shouldBe("StripSpaces(Math.cos.toString())","\"functioncos(){[nativecode]}\"");
shouldBe("StripSpaces(Math.exp.toString())","\"functionexp(){[nativecode]}\"");
shouldBe("StripSpaces(Math.floor.toString())","\"functionfloor(){[nativecode]}\"");
shouldBe("StripSpaces(Math.log.toString())","\"functionlog(){[nativecode]}\"");
shouldBe("StripSpaces(Math.max.toString())","\"functionmax(){[nativecode]}\"");
shouldBe("StripSpaces(Math.min.toString())","\"functionmin(){[nativecode]}\"");
shouldBe("StripSpaces(Math.pow.toString())","\"functionpow(){[nativecode]}\"");
shouldBe("StripSpaces(Math.random.toString())","\"functionrandom(){[nativecode]}\"");
shouldBe("StripSpaces(Math.round.toString())","\"functionround(){[nativecode]}\"");
shouldBe("StripSpaces(Math.sin.toString())","\"functionsin(){[nativecode]}\"");
shouldBe("StripSpaces(Math.sqrt.toString())","\"functionsqrt(){[nativecode]}\"");
shouldBe("StripSpaces(Math.tan.toString())","\"functiontan(){[nativecode]}\"");

shouldBe("StripSpaces(Date.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toUTCString.toString())","\"functiontoUTCString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toDateString.toString())","\"functiontoDateString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toTimeString.toString())","\"functiontoTimeString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toLocaleString.toString())","\"functiontoLocaleString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toLocaleDateString.toString())","\"functiontoLocaleDateString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toLocaleTimeString.toString())","\"functiontoLocaleTimeString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.valueOf.toString())","\"functionvalueOf(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getTime.toString())","\"functiongetTime(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getFullYear.toString())","\"functiongetFullYear(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCFullYear.toString())","\"functiongetUTCFullYear(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toGMTString.toString())","\"functiontoGMTString(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getMonth.toString())","\"functiongetMonth(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCMonth.toString())","\"functiongetUTCMonth(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getDate.toString())","\"functiongetDate(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCDate.toString())","\"functiongetUTCDate(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getDay.toString())","\"functiongetDay(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCDay.toString())","\"functiongetUTCDay(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getHours.toString())","\"functiongetHours(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCHours.toString())","\"functiongetUTCHours(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getMinutes.toString())","\"functiongetMinutes(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCMinutes.toString())","\"functiongetUTCMinutes(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getSeconds.toString())","\"functiongetSeconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCSeconds.toString())","\"functiongetUTCSeconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getMilliseconds.toString())","\"functiongetMilliseconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getUTCMilliseconds.toString())","\"functiongetUTCMilliseconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getTimezoneOffset.toString())","\"functiongetTimezoneOffset(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setTime.toString())","\"functionsetTime(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setMilliseconds.toString())","\"functionsetMilliseconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCMilliseconds.toString())","\"functionsetUTCMilliseconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setSeconds.toString())","\"functionsetSeconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCSeconds.toString())","\"functionsetUTCSeconds(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setMinutes.toString())","\"functionsetMinutes(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCMinutes.toString())","\"functionsetUTCMinutes(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setHours.toString())","\"functionsetHours(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCHours.toString())","\"functionsetUTCHours(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setDate.toString())","\"functionsetDate(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCDate.toString())","\"functionsetUTCDate(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setMonth.toString())","\"functionsetMonth(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCMonth.toString())","\"functionsetUTCMonth(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setFullYear.toString())","\"functionsetFullYear(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setUTCFullYear.toString())","\"functionsetUTCFullYear(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.setYear.toString())","\"functionsetYear(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.getYear.toString())","\"functiongetYear(){[nativecode]}\"");
shouldBe("StripSpaces(Date.prototype.toGMTString.toString())","\"functiontoGMTString(){[nativecode]}\"");

shouldBe("StripSpaces(RegExp.prototype.exec.toString())","\"functionexec(){[nativecode]}\"");
shouldBe("StripSpaces(RegExp.prototype.test.toString())","\"functiontest(){[nativecode]}\"");
shouldBe("StripSpaces(RegExp.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");

shouldBe("StripSpaces(Error.prototype.toString.toString())","\"functiontoString(){[nativecode]}\"");
