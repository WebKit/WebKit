//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Deseret alphabet toUpperCase",
    body: function () {
        assert.areEqual("\uD801\uDC00", "\uD801\uDC28".toUpperCase(), "Expecting Deseret alphabet upper-case long I");
        assert.areEqual("\uD801\uDC01", "\uD801\uDC29".toUpperCase(), "Expecting Deseret alphabet upper-case long E");
        assert.areEqual("\uD801\uDC02", "\uD801\uDC2A".toUpperCase(), "Expecting Deseret alphabet upper-case long A");
        assert.areEqual("\uD801\uDC03", "\uD801\uDC2B".toUpperCase(), "Expecting Deseret alphabet upper-case long Ah");
        assert.areEqual("\uD801\uDC04", "\uD801\uDC2C".toUpperCase(), "Expecting Deseret alphabet upper-case long O");
        assert.areEqual("\uD801\uDC05", "\uD801\uDC2D".toUpperCase(), "Expecting Deseret alphabet upper-case long Oo");
        assert.areEqual("\uD801\uDC06", "\uD801\uDC2E".toUpperCase(), "Expecting Deseret alphabet upper-case short I");
        assert.areEqual("\uD801\uDC07", "\uD801\uDC2F".toUpperCase(), "Expecting Deseret alphabet upper-case short E");
        assert.areEqual("\uD801\uDC08", "\uD801\uDC30".toUpperCase(), "Expecting Deseret alphabet upper-case short A");
        assert.areEqual("\uD801\uDC09", "\uD801\uDC31".toUpperCase(), "Expecting Deseret alphabet upper-case short Ah");
        assert.areEqual("\uD801\uDC0A", "\uD801\uDC32".toUpperCase(), "Expecting Deseret alphabet upper-case short O");
        assert.areEqual("\uD801\uDC0B", "\uD801\uDC33".toUpperCase(), "Expecting Deseret alphabet upper-case short Oo");
        assert.areEqual("\uD801\uDC0C", "\uD801\uDC34".toUpperCase(), "Expecting Deseret alphabet upper-case Ay");
        assert.areEqual("\uD801\uDC0D", "\uD801\uDC35".toUpperCase(), "Expecting Deseret alphabet upper-case Ow");
        assert.areEqual("\uD801\uDC0E", "\uD801\uDC36".toUpperCase(), "Expecting Deseret alphabet upper-case Wu");
        assert.areEqual("\uD801\uDC0F", "\uD801\uDC37".toUpperCase(), "Expecting Deseret alphabet upper-case Yee");
        assert.areEqual("\uD801\uDC10", "\uD801\uDC38".toUpperCase(), "Expecting Deseret alphabet upper-case H");
        assert.areEqual("\uD801\uDC11", "\uD801\uDC39".toUpperCase(), "Expecting Deseret alphabet upper-case Pee");
        assert.areEqual("\uD801\uDC12", "\uD801\uDC3A".toUpperCase(), "Expecting Deseret alphabet upper-case Bee");
        assert.areEqual("\uD801\uDC13", "\uD801\uDC3B".toUpperCase(), "Expecting Deseret alphabet upper-case Tee");
        assert.areEqual("\uD801\uDC14", "\uD801\uDC3C".toUpperCase(), "Expecting Deseret alphabet upper-case Dee");
        assert.areEqual("\uD801\uDC15", "\uD801\uDC3D".toUpperCase(), "Expecting Deseret alphabet upper-case Chee");
        assert.areEqual("\uD801\uDC16", "\uD801\uDC3E".toUpperCase(), "Expecting Deseret alphabet upper-case Jee");
        assert.areEqual("\uD801\uDC17", "\uD801\uDC3F".toUpperCase(), "Expecting Deseret alphabet upper-case Kay");
        assert.areEqual("\uD801\uDC18", "\uD801\uDC40".toUpperCase(), "Expecting Deseret alphabet upper-case Gay");
        assert.areEqual("\uD801\uDC19", "\uD801\uDC41".toUpperCase(), "Expecting Deseret alphabet upper-case Ef");
        assert.areEqual("\uD801\uDC1A", "\uD801\uDC42".toUpperCase(), "Expecting Deseret alphabet upper-case Vee");
        assert.areEqual("\uD801\uDC1B", "\uD801\uDC43".toUpperCase(), "Expecting Deseret alphabet upper-case Eth");
        assert.areEqual("\uD801\uDC1C", "\uD801\uDC44".toUpperCase(), "Expecting Deseret alphabet upper-case Thee");
        assert.areEqual("\uD801\uDC1D", "\uD801\uDC45".toUpperCase(), "Expecting Deseret alphabet upper-case Es");
        assert.areEqual("\uD801\uDC1E", "\uD801\uDC46".toUpperCase(), "Expecting Deseret alphabet upper-case Zee");
        assert.areEqual("\uD801\uDC1F", "\uD801\uDC47".toUpperCase(), "Expecting Deseret alphabet upper-case Esh");
        assert.areEqual("\uD801\uDC20", "\uD801\uDC48".toUpperCase(), "Expecting Deseret alphabet upper-case Zhee");
        assert.areEqual("\uD801\uDC21", "\uD801\uDC49".toUpperCase(), "Expecting Deseret alphabet upper-case Er");
        assert.areEqual("\uD801\uDC22", "\uD801\uDC4A".toUpperCase(), "Expecting Deseret alphabet upper-case El");
        assert.areEqual("\uD801\uDC23", "\uD801\uDC4B".toUpperCase(), "Expecting Deseret alphabet upper-case Em");
        assert.areEqual("\uD801\uDC24", "\uD801\uDC4C".toUpperCase(), "Expecting Deseret alphabet upper-case En");
        assert.areEqual("\uD801\uDC25", "\uD801\uDC4D".toUpperCase(), "Expecting Deseret alphabet upper-case Eng");
        assert.areEqual("\uD801\uDC26", "\uD801\uDC4E".toUpperCase(), "Expecting Deseret alphabet upper-case Oi");
        assert.areEqual("\uD801\uDC27", "\uD801\uDC4F".toUpperCase(), "Expecting Deseret alphabet upper-case Ew");
    }
  },
  {
    name: "Deseret alphabet toLowerCase",
    body: function () {
        assert.areEqual("\uD801\uDC28", "\uD801\uDC00".toLowerCase(), "Expecting Deseret alphabet lower-case long I");
        assert.areEqual("\uD801\uDC29", "\uD801\uDC01".toLowerCase(), "Expecting Deseret alphabet lower-case long E");
        assert.areEqual("\uD801\uDC2A", "\uD801\uDC02".toLowerCase(), "Expecting Deseret alphabet lower-case long A");
        assert.areEqual("\uD801\uDC2B", "\uD801\uDC03".toLowerCase(), "Expecting Deseret alphabet lower-case long Ah");
        assert.areEqual("\uD801\uDC2C", "\uD801\uDC04".toLowerCase(), "Expecting Deseret alphabet lower-case long O");
        assert.areEqual("\uD801\uDC2D", "\uD801\uDC05".toLowerCase(), "Expecting Deseret alphabet lower-case long Oo");
        assert.areEqual("\uD801\uDC2E", "\uD801\uDC06".toLowerCase(), "Expecting Deseret alphabet lower-case short I");
        assert.areEqual("\uD801\uDC2F", "\uD801\uDC07".toLowerCase(), "Expecting Deseret alphabet lower-case short E");
        assert.areEqual("\uD801\uDC30", "\uD801\uDC08".toLowerCase(), "Expecting Deseret alphabet lower-case short A");
        assert.areEqual("\uD801\uDC31", "\uD801\uDC09".toLowerCase(), "Expecting Deseret alphabet lower-case short Ah");
        assert.areEqual("\uD801\uDC32", "\uD801\uDC0A".toLowerCase(), "Expecting Deseret alphabet lower-case short O");
        assert.areEqual("\uD801\uDC33", "\uD801\uDC0B".toLowerCase(), "Expecting Deseret alphabet lower-case short Oo");
        assert.areEqual("\uD801\uDC34", "\uD801\uDC0C".toLowerCase(), "Expecting Deseret alphabet lower-case Ay");
        assert.areEqual("\uD801\uDC35", "\uD801\uDC0D".toLowerCase(), "Expecting Deseret alphabet lower-case Ow");
        assert.areEqual("\uD801\uDC36", "\uD801\uDC0E".toLowerCase(), "Expecting Deseret alphabet lower-case Wu");
        assert.areEqual("\uD801\uDC37", "\uD801\uDC0F".toLowerCase(), "Expecting Deseret alphabet lower-case Yee");
        assert.areEqual("\uD801\uDC38", "\uD801\uDC10".toLowerCase(), "Expecting Deseret alphabet lower-case H");
        assert.areEqual("\uD801\uDC39", "\uD801\uDC11".toLowerCase(), "Expecting Deseret alphabet lower-case Pee");
        assert.areEqual("\uD801\uDC3A", "\uD801\uDC12".toLowerCase(), "Expecting Deseret alphabet lower-case Bee");
        assert.areEqual("\uD801\uDC3B", "\uD801\uDC13".toLowerCase(), "Expecting Deseret alphabet lower-case Tee");
        assert.areEqual("\uD801\uDC3C", "\uD801\uDC14".toLowerCase(), "Expecting Deseret alphabet lower-case Dee");
        assert.areEqual("\uD801\uDC3D", "\uD801\uDC15".toLowerCase(), "Expecting Deseret alphabet lower-case Chee");
        assert.areEqual("\uD801\uDC3E", "\uD801\uDC16".toLowerCase(), "Expecting Deseret alphabet lower-case Jee");
        assert.areEqual("\uD801\uDC3F", "\uD801\uDC17".toLowerCase(), "Expecting Deseret alphabet lower-case Kay");
        assert.areEqual("\uD801\uDC40", "\uD801\uDC18".toLowerCase(), "Expecting Deseret alphabet lower-case Gay");
        assert.areEqual("\uD801\uDC41", "\uD801\uDC19".toLowerCase(), "Expecting Deseret alphabet lower-case Ef");
        assert.areEqual("\uD801\uDC42", "\uD801\uDC1A".toLowerCase(), "Expecting Deseret alphabet lower-case Vee");
        assert.areEqual("\uD801\uDC43", "\uD801\uDC1B".toLowerCase(), "Expecting Deseret alphabet lower-case Eth");
        assert.areEqual("\uD801\uDC44", "\uD801\uDC1C".toLowerCase(), "Expecting Deseret alphabet lower-case Thee");
        assert.areEqual("\uD801\uDC45", "\uD801\uDC1D".toLowerCase(), "Expecting Deseret alphabet lower-case Es");
        assert.areEqual("\uD801\uDC46", "\uD801\uDC1E".toLowerCase(), "Expecting Deseret alphabet lower-case Zee");
        assert.areEqual("\uD801\uDC47", "\uD801\uDC1F".toLowerCase(), "Expecting Deseret alphabet lower-case Esh");
        assert.areEqual("\uD801\uDC48", "\uD801\uDC20".toLowerCase(), "Expecting Deseret alphabet lower-case Zhee");
        assert.areEqual("\uD801\uDC49", "\uD801\uDC21".toLowerCase(), "Expecting Deseret alphabet lower-case Er");
        assert.areEqual("\uD801\uDC4A", "\uD801\uDC22".toLowerCase(), "Expecting Deseret alphabet lower-case El");
        assert.areEqual("\uD801\uDC4B", "\uD801\uDC23".toLowerCase(), "Expecting Deseret alphabet lower-case Em");
        assert.areEqual("\uD801\uDC4C", "\uD801\uDC24".toLowerCase(), "Expecting Deseret alphabet lower-case En");
        assert.areEqual("\uD801\uDC4D", "\uD801\uDC25".toLowerCase(), "Expecting Deseret alphabet lower-case Eng");
        assert.areEqual("\uD801\uDC4E", "\uD801\uDC26".toLowerCase(), "Expecting Deseret alphabet lower-case Oi");
        assert.areEqual("\uD801\uDC4F", "\uD801\uDC27".toLowerCase(), "Expecting Deseret alphabet lower-case Ew");
    }
  },
  {
    name: "Special casing toUpperCase",
    body: function () {
        //assert.areEqual("\u0053\u0053", "\u00DF".toUpperCase(), "Expecting Latin lower-case sharp s"); // not yet implemented
        assert.areEqual("\u0130", "\u0130".toUpperCase(), "Expecting Latin upper-case i with dot above");
        //assert.areEqual("\u0046\u0046", "\uFB00".toUpperCase(), "Expecting Latin small ligature ff"); // not yet implemented
        //assert.areEqual("\u0046\u0049", "\uFB01".toUpperCase(), "Expecting Latin small ligature fi"); // not yet implemented
        //assert.areEqual("\u0046\u004C", "\uFB02".toUpperCase(), "Expecting Latin small ligature fl"); // not yet implemented
        //assert.areEqual("\u0046\u0046\u0049", "\uFB03".toUpperCase(), "Expecting Latin small ligature ffi"); // not yet implemented
        //assert.areEqual("\u0046\u0046\u004C", "\uFB04".toUpperCase(), "Expecting Latin small ligature ffl"); // not yet implemented
        //assert.areEqual("\u0053\u0054", "\uFB05".toUpperCase(), "Expecting Latin small ligature long s t"); // not yet implemented
        //assert.areEqual("\u0053\u0054", "\uFB06".toUpperCase(), "Expecting Latin small ligature st"); // not yet implemented
        //assert.areEqual("\u0535\u0552", "\u0587".toUpperCase(), "Expecting Armenian small ligature ech yiwn"); // not yet implemented
        //assert.areEqual("\u0544\u0546", "\uFB13".toUpperCase(), "Expecting Armenian small ligature men now"); // not yet implemented
        //assert.areEqual("\u0544\u0535", "\uFB14".toUpperCase(), "Expecting Armenian small ligature men ech"); // not yet implemented
        //assert.areEqual("\u0544\u053B", "\uFB15".toUpperCase(), "Expecting Armenian small ligature men ini"); // not yet implemented
        //assert.areEqual("\u054E\u0546", "\uFB16".toUpperCase(), "Expecting Armenian small ligature vew now"); // not yet implemented
        //assert.areEqual("\u0544\u053D", "\uFB17".toUpperCase(), "Expecting Armenian small ligature men xeh"); // not yet implemented
        //assert.areEqual("\u02BC\u004E", "\u0149".toUpperCase(), "Expecting Latin lower-case n preceded by apostrophe"); // not yet implemented
        //assert.areEqual("\u0399\u0308\u0301", "\u0390".toUpperCase(), "Expecting Greek lower-case iota with dialytika and tonos"); // not yet implemented
        //assert.areEqual("\u03A5\u0308\u0301", "\u03B0".toUpperCase(), "Expecting Greek lower-case upsilon with dialytika and tonos"); // not yet implemented
        //assert.areEqual("\u004A\u030C", "\u01F0".toUpperCase(), "Expecting Latin lower-case j with caron"); // not yet implemented
        //assert.areEqual("\u0048\u0331", "\u1E96".toUpperCase(), "Expecting Latin lower-case h with line below"); // not yet implemented
        //assert.areEqual("\u0054\u0308", "\u1E97".toUpperCase(), "Expecting Latin lower-case t with diaeresis"); // not yet implemented
        //assert.areEqual("\u0057\u030A", "\u1E98".toUpperCase(), "Expecting Latin lower-case w with ring above"); // not yet implemented
        //assert.areEqual("\u0059\u030A", "\u1E99".toUpperCase(), "Expecting Latin lower-case y with ring above"); // not yet implemented
        //assert.areEqual("\u0041\u02BE", "\u1E9A".toUpperCase(), "Expecting Latin lower-case a with right half ring"); // not yet implemented
        //assert.areEqual("\u03A5\u0313", "\u1F50".toUpperCase(), "Expecting Greek lower-case upsilon with psili"); // not yet implemented
        //assert.areEqual("\u03A5\u0313\u0300", "\u1F52".toUpperCase(), "Expecting Greek lower-case upsilon with psili and varia"); // not yet implemented
        //assert.areEqual("\u03A5\u0313\u0301", "\u1F54".toUpperCase(), "Expecting Greek lower-case upsilon with psili and oxia"); // not yet implemented
        //assert.areEqual("\u03A5\u0313\u0342", "\u1F56".toUpperCase(), "Expecting Greek lower-case upsilon with psili and perispomeni"); // not yet implemented
        //assert.areEqual("\u0391\u0342", "\u1FB6".toUpperCase(), "Expecting Greek lower-case alpha with perispomeni"); // not yet implemented
        //assert.areEqual("\u0397\u0342", "\u1FC6".toUpperCase(), "Expecting Greek lower-case eta with perispomeni"); // not yet implemented
        //assert.areEqual("\u0399\u0308\u0300", "\u1FD2".toUpperCase(), "Expecting Greek lower-case iota with dialytika and varia"); // not yet implemented
        //assert.areEqual("\u0399\u0308\u0301", "\u1FD3".toUpperCase(), "Expecting Greek lower-case iota with dialytika and oxia"); // not yet implemented
        //assert.areEqual("\u0399\u0342", "\u1FD6".toUpperCase(), "Expecting Greek lower-case iota with perispomeni"); // not yet implemented
        //assert.areEqual("\u0399\u0308\u0342", "\u1FD7".toUpperCase(), "Expecting Greek lower-case iota with dialytika and perispomeni"); // not yet implemented
        //assert.areEqual("\u03A5\u0308\u0300", "\u1FE2".toUpperCase(), "Expecting Greek lower-case upsilon with dialytika and varia"); // not yet implemented
        //assert.areEqual("\u03A5\u0308\u0301", "\u1FE3".toUpperCase(), "Expecting Greek lower-case upsilon with dialytika and oxia"); // not yet implemented
        //assert.areEqual("\u03A1\u0313", "\u1FE4".toUpperCase(), "Expecting Greek lower-case rho with psili"); // not yet implemented
        //assert.areEqual("\u03A5\u0342", "\u1FE6".toUpperCase(), "Expecting Greek lower-case upsilon with perispomeni"); // not yet implemented
        //assert.areEqual("\u03A5\u0308\u0342", "\u1FE7".toUpperCase(), "Expecting Greek lower-case upsilon with dialytika and perispomeni"); // not yet implemented
        //assert.areEqual("\u03A9\u0342", "\u1FF6".toUpperCase(), "Expecting Greek lower-case omega with perispomeni"); // not yet implemented
        //assert.areEqual("\u1F08\u0399", "\u1F80".toUpperCase(), "Expecting Greek lower-case alpha with psili and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F09\u0399", "\u1F81".toUpperCase(), "Expecting Greek lower-case alpha with dasia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0A\u0399", "\u1F82".toUpperCase(), "Expecting Greek lower-case alpha with psili and varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0B\u0399", "\u1F83".toUpperCase(), "Expecting Greek lower-case alpha with dasia and varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0C\u0399", "\u1F84".toUpperCase(), "Expecting Greek lower-case alpha with psili and oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0D\u0399", "\u1F85".toUpperCase(), "Expecting Greek lower-case alpha with dasia and oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0E\u0399", "\u1F86".toUpperCase(), "Expecting Greek lower-case alpha with psili and perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0F\u0399", "\u1F87".toUpperCase(), "Expecting Greek lower-case alpha with dasia and perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F08\u0399", "\u1F88".toUpperCase(), "Expecting Greek upper-case alpha with psili and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F09\u0399", "\u1F89".toUpperCase(), "Expecting Greek upper-case alpha with dasia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0A\u0399", "\u1F8A".toUpperCase(), "Expecting Greek upper-case alpha with psili and varia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0B\u0399", "\u1F8B".toUpperCase(), "Expecting Greek upper-case alpha with dasia and varia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0C\u0399", "\u1F8C".toUpperCase(), "Expecting Greek upper-case alpha with psili and oxia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0D\u0399", "\u1F8D".toUpperCase(), "Expecting Greek upper-case alpha with dasia and oxia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0E\u0399", "\u1F8E".toUpperCase(), "Expecting Greek upper-case alpha with psili and perispomeni and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F0F\u0399", "\u1F8F".toUpperCase(), "Expecting Greek upper-case alpha with dasia and perispomeni and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F28\u0399", "\u1F90".toUpperCase(), "Expecting Greek lower-case eta with psili and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F29\u0399", "\u1F91".toUpperCase(), "Expecting Greek lower-case eta with dasia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2A\u0399", "\u1F92".toUpperCase(), "Expecting Greek lower-case eta with psili and varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2B\u0399", "\u1F93".toUpperCase(), "Expecting Greek lower-case eta with dasia and varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2C\u0399", "\u1F94".toUpperCase(), "Expecting Greek lower-case eta with psili and oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2D\u0399", "\u1F95".toUpperCase(), "Expecting Greek lower-case eta with dasia and oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2E\u0399", "\u1F96".toUpperCase(), "Expecting Greek lower-case eta with psili and perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2F\u0399", "\u1F97".toUpperCase(), "Expecting Greek lower-case eta with dasia and perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F28\u0399", "\u1F98".toUpperCase(), "Expecting Greek upper-case eta with psili and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F29\u0399", "\u1F99".toUpperCase(), "Expecting Greek upper-case eta with dasia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2A\u0399", "\u1F9A".toUpperCase(), "Expecting Greek upper-case eta with psili and varia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2B\u0399", "\u1F9B".toUpperCase(), "Expecting Greek upper-case eta with dasia and varia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2C\u0399", "\u1F9C".toUpperCase(), "Expecting Greek upper-case eta with psili and oxia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2D\u0399", "\u1F9D".toUpperCase(), "Expecting Greek upper-case eta with dasia and oxia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2E\u0399", "\u1F9E".toUpperCase(), "Expecting Greek upper-case eta with psili and perispomeni and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F2F\u0399", "\u1F9F".toUpperCase(), "Expecting Greek upper-case eta with dasia and perispomeni and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F68\u0399", "\u1FA0".toUpperCase(), "Expecting Greek lower-case omega with psili and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F69\u0399", "\u1FA1".toUpperCase(), "Expecting Greek lower-case omega with dasia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6A\u0399", "\u1FA2".toUpperCase(), "Expecting Greek lower-case omega with psili and varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6B\u0399", "\u1FA3".toUpperCase(), "Expecting Greek lower-case omega with dasia and varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6C\u0399", "\u1FA4".toUpperCase(), "Expecting Greek lower-case omega with psili and oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6D\u0399", "\u1FA5".toUpperCase(), "Expecting Greek lower-case omega with dasia and oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6E\u0399", "\u1FA6".toUpperCase(), "Expecting Greek lower-case omega with psili and perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6F\u0399", "\u1FA7".toUpperCase(), "Expecting Greek lower-case omega with dasia and perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1F68\u0399", "\u1FA8".toUpperCase(), "Expecting Greek upper-case omega with psili and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F69\u0399", "\u1FA9".toUpperCase(), "Expecting Greek upper-case omega with dasia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6A\u0399", "\u1FAA".toUpperCase(), "Expecting Greek upper-case omega with psili and varia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6B\u0399", "\u1FAB".toUpperCase(), "Expecting Greek upper-case omega with dasia and varia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6C\u0399", "\u1FAC".toUpperCase(), "Expecting Greek upper-case omega with psili and oxia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6D\u0399", "\u1FAD".toUpperCase(), "Expecting Greek upper-case omega with dasia and oxia and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6E\u0399", "\u1FAE".toUpperCase(), "Expecting Greek upper-case omega with psili and perispomeni and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1F6F\u0399", "\u1FAF".toUpperCase(), "Expecting Greek upper-case omega with dasia and perispomeni and prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u0391\u0399", "\u1FB3".toUpperCase(), "Expecting Greek lower-case alpha with ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u0391\u0399", "\u1FBC".toUpperCase(), "Expecting Greek upper-case alpha with prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u0397\u0399", "\u1FC3".toUpperCase(), "Expecting Greek lower-case eta with ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u0397\u0399", "\u1FCC".toUpperCase(), "Expecting Greek upper-case eta with prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u03A9\u0399", "\u1FF3".toUpperCase(), "Expecting Greek lower-case omega with ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u03A9\u0399", "\u1FFC".toUpperCase(), "Expecting Greek upper-case omega with prosgegrammeni"); // not yet implemented
        //assert.areEqual("\u1FBA\u0399", "\u1FB2".toUpperCase(), "Expecting Greek lower-case alpha with varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u0386\u0399", "\u1FB4".toUpperCase(), "Expecting Greek lower-case alpha with oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1FCA\u0399", "\u1FC2".toUpperCase(), "Expecting Greek lower-case eta with varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u0389\u0399", "\u1FC4".toUpperCase(), "Expecting Greek lower-case eta with oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u1FFA\u0399", "\u1FF2".toUpperCase(), "Expecting Greek lower-case omega with varia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u038F\u0399", "\u1FF4".toUpperCase(), "Expecting Greek lower-case omega with oxia and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u0391\u0342\u0399", "\u1FB7".toUpperCase(), "Expecting Greek lower-case alpha with perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u0397\u0342\u0399", "\u1FC7".toUpperCase(), "Expecting Greek lower-case eta with perispomeni and ypogegrammeni"); // not yet implemented
        //assert.areEqual("\u03A9\u0342\u0399", "\u1FF7".toUpperCase(), "Expecting Greek lower-case omega with perispomeni and ypogegrammeni"); // not yet implemented
    }
  },
  {
    name: "Special casing toLowerCase",
    body: function () {
        assert.areEqual("\u00DF", "\u00DF".toLowerCase(), "Expecting Latin lower-case sharp s");
        //assert.areEqual("\u0069\u0307", "\u0130".toLowerCase(), "Expecting Latin upper-case i with dot above"); // not yet implemented
        assert.areEqual("\uFB00", "\uFB00".toLowerCase(), "Expecting Latin small ligature ff");
        assert.areEqual("\uFB01", "\uFB01".toLowerCase(), "Expecting Latin small ligature fi");
        assert.areEqual("\uFB02", "\uFB02".toLowerCase(), "Expecting Latin small ligature fl");
        assert.areEqual("\uFB03", "\uFB03".toLowerCase(), "Expecting Latin small ligature ffi");
        assert.areEqual("\uFB04", "\uFB04".toLowerCase(), "Expecting Latin small ligature ffl");
        assert.areEqual("\uFB05", "\uFB05".toLowerCase(), "Expecting Latin small ligature long s t");
        assert.areEqual("\uFB06", "\uFB06".toLowerCase(), "Expecting Latin small ligature st");
        assert.areEqual("\u0587", "\u0587".toLowerCase(), "Expecting Armenian small ligature ech yiwn");
        assert.areEqual("\uFB13", "\uFB13".toLowerCase(), "Expecting Armenian small ligature men now");
        assert.areEqual("\uFB14", "\uFB14".toLowerCase(), "Expecting Armenian small ligature men ech");
        assert.areEqual("\uFB15", "\uFB15".toLowerCase(), "Expecting Armenian small ligature men ini");
        assert.areEqual("\uFB16", "\uFB16".toLowerCase(), "Expecting Armenian small ligature vew now");
        assert.areEqual("\uFB17", "\uFB17".toLowerCase(), "Expecting Armenian small ligature men xeh");
        assert.areEqual("\u0149", "\u0149".toLowerCase(), "Expecting Latin lower-case n preceded by apostrophe");
        assert.areEqual("\u0390", "\u0390".toLowerCase(), "Expecting Greek lower-case iota with dialytika and tonos");
        assert.areEqual("\u03B0", "\u03B0".toLowerCase(), "Expecting Greek lower-case upsilon with dialytika and tonos");
        assert.areEqual("\u01F0", "\u01F0".toLowerCase(), "Expecting Latin lower-case j with caron");
        assert.areEqual("\u1E96", "\u1E96".toLowerCase(), "Expecting Latin lower-case h with line below");
        assert.areEqual("\u1E97", "\u1E97".toLowerCase(), "Expecting Latin lower-case t with diaeresis");
        assert.areEqual("\u1E98", "\u1E98".toLowerCase(), "Expecting Latin lower-case w with ring above");
        assert.areEqual("\u1E99", "\u1E99".toLowerCase(), "Expecting Latin lower-case y with ring above");
        assert.areEqual("\u1E9A", "\u1E9A".toLowerCase(), "Expecting Latin lower-case a with right half ring");
        assert.areEqual("\u1F50", "\u1F50".toLowerCase(), "Expecting Greek lower-case upsilon with psili");
        assert.areEqual("\u1F52", "\u1F52".toLowerCase(), "Expecting Greek lower-case upsilon with psili and varia");
        assert.areEqual("\u1F54", "\u1F54".toLowerCase(), "Expecting Greek lower-case upsilon with psili and oxia");
        assert.areEqual("\u1F56", "\u1F56".toLowerCase(), "Expecting Greek lower-case upsilon with psili and perispomeni");
        assert.areEqual("\u1FB6", "\u1FB6".toLowerCase(), "Expecting Greek lower-case alpha with perispomeni");
        assert.areEqual("\u1FC6", "\u1FC6".toLowerCase(), "Expecting Greek lower-case eta with perispomeni");
        assert.areEqual("\u1FD2", "\u1FD2".toLowerCase(), "Expecting Greek lower-case iota with dialytika and varia");
        assert.areEqual("\u1FD3", "\u1FD3".toLowerCase(), "Expecting Greek lower-case iota with dialytika and oxia");
        assert.areEqual("\u1FD6", "\u1FD6".toLowerCase(), "Expecting Greek lower-case iota with perispomeni");
        assert.areEqual("\u1FD7", "\u1FD7".toLowerCase(), "Expecting Greek lower-case iota with dialytika and perispomeni");
        assert.areEqual("\u1FE2", "\u1FE2".toLowerCase(), "Expecting Greek lower-case upsilon with dialytika and varia");
        assert.areEqual("\u1FE3", "\u1FE3".toLowerCase(), "Expecting Greek lower-case upsilon with dialytika and oxia");
        assert.areEqual("\u1FE4", "\u1FE4".toLowerCase(), "Expecting Greek lower-case rho with psili");
        assert.areEqual("\u1FE6", "\u1FE6".toLowerCase(), "Expecting Greek lower-case upsilon with perispomeni");
        assert.areEqual("\u1FE7", "\u1FE7".toLowerCase(), "Expecting Greek lower-case upsilon with dialytika and perispomeni");
        assert.areEqual("\u1FF6", "\u1FF6".toLowerCase(), "Expecting Greek lower-case omega with perispomeni");
        assert.areEqual("\u1F80", "\u1F80".toLowerCase(), "Expecting Greek lower-case alpha with psili and ypogegrammeni");
        assert.areEqual("\u1F81", "\u1F81".toLowerCase(), "Expecting Greek lower-case alpha with dasia and ypogegrammeni");
        assert.areEqual("\u1F82", "\u1F82".toLowerCase(), "Expecting Greek lower-case alpha with psili and varia and ypogegrammeni");
        assert.areEqual("\u1F83", "\u1F83".toLowerCase(), "Expecting Greek lower-case alpha with dasia and varia and ypogegrammeni");
        assert.areEqual("\u1F84", "\u1F84".toLowerCase(), "Expecting Greek lower-case alpha with psili and oxia and ypogegrammeni");
        assert.areEqual("\u1F85", "\u1F85".toLowerCase(), "Expecting Greek lower-case alpha with dasia and oxia and ypogegrammeni");
        assert.areEqual("\u1F86", "\u1F86".toLowerCase(), "Expecting Greek lower-case alpha with psili and perispomeni and ypogegrammeni");
        assert.areEqual("\u1F87", "\u1F87".toLowerCase(), "Expecting Greek lower-case alpha with dasia and perispomeni and ypogegrammeni");
        assert.areEqual("\u1F80", "\u1F88".toLowerCase(), "Expecting Greek upper-case alpha with psili and prosgegrammeni");
        assert.areEqual("\u1F81", "\u1F89".toLowerCase(), "Expecting Greek upper-case alpha with dasia and prosgegrammeni");
        assert.areEqual("\u1F82", "\u1F8A".toLowerCase(), "Expecting Greek upper-case alpha with psili and varia and prosgegrammeni");
        assert.areEqual("\u1F83", "\u1F8B".toLowerCase(), "Expecting Greek upper-case alpha with dasia and varia and prosgegrammeni");
        assert.areEqual("\u1F84", "\u1F8C".toLowerCase(), "Expecting Greek upper-case alpha with psili and oxia and prosgegrammeni");
        assert.areEqual("\u1F85", "\u1F8D".toLowerCase(), "Expecting Greek upper-case alpha with dasia and oxia and prosgegrammeni");
        assert.areEqual("\u1F86", "\u1F8E".toLowerCase(), "Expecting Greek upper-case alpha with psili and perispomeni and prosgegrammeni");
        assert.areEqual("\u1F87", "\u1F8F".toLowerCase(), "Expecting Greek upper-case alpha with dasia and perispomeni and prosgegrammeni");
        assert.areEqual("\u1F90", "\u1F90".toLowerCase(), "Expecting Greek lower-case eta with psili and ypogegrammeni");
        assert.areEqual("\u1F91", "\u1F91".toLowerCase(), "Expecting Greek lower-case eta with dasia and ypogegrammeni");
        assert.areEqual("\u1F92", "\u1F92".toLowerCase(), "Expecting Greek lower-case eta with psili and varia and ypogegrammeni");
        assert.areEqual("\u1F93", "\u1F93".toLowerCase(), "Expecting Greek lower-case eta with dasia and varia and ypogegrammeni");
        assert.areEqual("\u1F94", "\u1F94".toLowerCase(), "Expecting Greek lower-case eta with psili and oxia and ypogegrammeni");
        assert.areEqual("\u1F95", "\u1F95".toLowerCase(), "Expecting Greek lower-case eta with dasia and oxia and ypogegrammeni");
        assert.areEqual("\u1F96", "\u1F96".toLowerCase(), "Expecting Greek lower-case eta with psili and perispomeni and ypogegrammeni");
        assert.areEqual("\u1F97", "\u1F97".toLowerCase(), "Expecting Greek lower-case eta with dasia and perispomeni and ypogegrammeni");
        assert.areEqual("\u1F90", "\u1F98".toLowerCase(), "Expecting Greek upper-case eta with psili and prosgegrammeni");
        assert.areEqual("\u1F91", "\u1F99".toLowerCase(), "Expecting Greek upper-case eta with dasia and prosgegrammeni");
        assert.areEqual("\u1F92", "\u1F9A".toLowerCase(), "Expecting Greek upper-case eta with psili and varia and prosgegrammeni");
        assert.areEqual("\u1F93", "\u1F9B".toLowerCase(), "Expecting Greek upper-case eta with dasia and varia and prosgegrammeni");
        assert.areEqual("\u1F94", "\u1F9C".toLowerCase(), "Expecting Greek upper-case eta with psili and oxia and prosgegrammeni");
        assert.areEqual("\u1F95", "\u1F9D".toLowerCase(), "Expecting Greek upper-case eta with dasia and oxia and prosgegrammeni");
        assert.areEqual("\u1F96", "\u1F9E".toLowerCase(), "Expecting Greek upper-case eta with psili and perispomeni and prosgegrammeni");
        assert.areEqual("\u1F97", "\u1F9F".toLowerCase(), "Expecting Greek upper-case eta with dasia and perispomeni and prosgegrammeni");
        assert.areEqual("\u1FA0", "\u1FA0".toLowerCase(), "Expecting Greek lower-case omega with psili and ypogegrammeni");
        assert.areEqual("\u1FA1", "\u1FA1".toLowerCase(), "Expecting Greek lower-case omega with dasia and ypogegrammeni");
        assert.areEqual("\u1FA2", "\u1FA2".toLowerCase(), "Expecting Greek lower-case omega with psili and varia and ypogegrammeni");
        assert.areEqual("\u1FA3", "\u1FA3".toLowerCase(), "Expecting Greek lower-case omega with dasia and varia and ypogegrammeni");
        assert.areEqual("\u1FA4", "\u1FA4".toLowerCase(), "Expecting Greek lower-case omega with psili and oxia and ypogegrammeni");
        assert.areEqual("\u1FA5", "\u1FA5".toLowerCase(), "Expecting Greek lower-case omega with dasia and oxia and ypogegrammeni");
        assert.areEqual("\u1FA6", "\u1FA6".toLowerCase(), "Expecting Greek lower-case omega with psili and perispomeni and ypogegrammeni");
        assert.areEqual("\u1FA7", "\u1FA7".toLowerCase(), "Expecting Greek lower-case omega with dasia and perispomeni and ypogegrammeni");
        assert.areEqual("\u1FA0", "\u1FA8".toLowerCase(), "Expecting Greek upper-case omega with psili and prosgegrammeni");
        assert.areEqual("\u1FA1", "\u1FA9".toLowerCase(), "Expecting Greek upper-case omega with dasia and prosgegrammeni");
        assert.areEqual("\u1FA2", "\u1FAA".toLowerCase(), "Expecting Greek upper-case omega with psili and varia and prosgegrammeni");
        assert.areEqual("\u1FA3", "\u1FAB".toLowerCase(), "Expecting Greek upper-case omega with dasia and varia and prosgegrammeni");
        assert.areEqual("\u1FA4", "\u1FAC".toLowerCase(), "Expecting Greek upper-case omega with psili and oxia and prosgegrammeni");
        assert.areEqual("\u1FA5", "\u1FAD".toLowerCase(), "Expecting Greek upper-case omega with dasia and oxia and prosgegrammeni");
        assert.areEqual("\u1FA6", "\u1FAE".toLowerCase(), "Expecting Greek upper-case omega with psili and perispomeni and prosgegrammeni");
        assert.areEqual("\u1FA7", "\u1FAF".toLowerCase(), "Expecting Greek upper-case omega with dasia and perispomeni and prosgegrammeni");
        assert.areEqual("\u1FB3", "\u1FB3".toLowerCase(), "Expecting Greek lower-case alpha with ypogegrammeni");
        assert.areEqual("\u1FB3", "\u1FBC".toLowerCase(), "Expecting Greek upper-case alpha with prosgegrammeni");
        assert.areEqual("\u1FC3", "\u1FC3".toLowerCase(), "Expecting Greek lower-case eta with ypogegrammeni");
        assert.areEqual("\u1FC3", "\u1FCC".toLowerCase(), "Expecting Greek upper-case eta with prosgegrammeni");
        assert.areEqual("\u1FF3", "\u1FF3".toLowerCase(), "Expecting Greek lower-case omega with ypogegrammeni");
        assert.areEqual("\u1FF3", "\u1FFC".toLowerCase(), "Expecting Greek upper-case omega with prosgegrammeni");
        assert.areEqual("\u1FB2", "\u1FB2".toLowerCase(), "Expecting Greek lower-case alpha with varia and ypogegrammeni");
        assert.areEqual("\u1FB4", "\u1FB4".toLowerCase(), "Expecting Greek lower-case alpha with oxia and ypogegrammeni");
        assert.areEqual("\u1FC2", "\u1FC2".toLowerCase(), "Expecting Greek lower-case eta with varia and ypogegrammeni");
        assert.areEqual("\u1FC4", "\u1FC4".toLowerCase(), "Expecting Greek lower-case eta with oxia and ypogegrammeni");
        assert.areEqual("\u1FF2", "\u1FF2".toLowerCase(), "Expecting Greek lower-case omega with varia and ypogegrammeni");
        assert.areEqual("\u1FF4", "\u1FF4".toLowerCase(), "Expecting Greek lower-case omega with oxia and ypogegrammeni");
        assert.areEqual("\u1FB7", "\u1FB7".toLowerCase(), "Expecting Greek lower-case alpha with perispomeni and ypogegrammeni");
        assert.areEqual("\u1FC7", "\u1FC7".toLowerCase(), "Expecting Greek lower-case eta with perispomeni and ypogegrammeni");
        assert.areEqual("\u1FF7", "\u1FF7".toLowerCase(), "Expecting Greek lower-case omega with perispomeni and ypogegrammeni");
        assert.areEqual("\u03C3", "\u03A3".toLowerCase(), "Expecting single Greek upper-case sigma");
        //assert.areEqual("a\u03C2", "A\u03A3".toLowerCase(), "Expecting sigma preceded by Latin upper-case a");  // not yet implemented
        //assert.areEqual("\uD835\uDCA2\u03C2", "\uD835\uDCA2\u03A3".toLowerCase(), "Expecting sigma preceded by mathematical script capital g (d835 dca2 = 1d4a2)");  // not yet implemented
        //assert.areEqual("a.\u03C2", "A.\u03A3".toLowerCase(), "Expecting sigma preceded by full stop");  // not yet implemented
        //assert.areEqual("a\u00AD\u03C2", "A\u00AD\u03A3".toLowerCase(), "Expecting sigma preceded by soft hyphen (00ad)");  // not yet implemented
        //assert.areEqual("a\uD834\uDE42\u03C2", "A\uD834\uDE42\u03A3".toLowerCase(), "Expecting sigma preceded by combining Greek musical triseme (d834 de42 = 1d242)");  // not yet implemented
        assert.areEqual("\u0345\u03C3", "\u0345\u03A3".toLowerCase(), "Expecting sigma preceded by combining Greek ypogegrammeni (0345)");
        //assert.areEqual("\u03B1\u0345\u03C2", "\u0391\u0345\u03A3".toLowerCase(), "Expecting sigma preceded by Greek upper-case alpha (0391), combining Greek ypogegrammeni (0345)");  // not yet implemented
        assert.areEqual("a\u03C3b", "A\u03A3B".toLowerCase(), "Expecting sigma followed by Latin upper-case b");
        assert.areEqual("a\u03C3\uD835\uDCA2", "A\u03A3\uD835\uDCA2".toLowerCase(), "Expecting sigma followed by mathematical script capital g (d835 dca2 = 1d4a2)");
        assert.areEqual("a\u03C3.b", "A\u03A3.b".toLowerCase(), "Expecting sigma followed by full stop");
        assert.areEqual("a\u03C3\u00ADb", "A\u03A3\u00ADB".toLowerCase(), "Expecting sigma followed by soft hyphen (00ad)");
        assert.areEqual("a\u03C3\uD834\uDE42b", "A\u03A3\uD834\uDE42B".toLowerCase(), "Expecting sigma followed by combining Greek musical triseme (d834 de42 = 1d242)");
        //assert.areEqual("a\u03C2\u0345", "A\u03A3\u0345".toLowerCase(), "Expecting sigma followed by combining Greek ypogegrammeni (0345)");  // not yet implemented
        assert.areEqual("a\u03C3\u0345\u03B1", "A\u03A3\u0345\u0391".toLowerCase(), "Expecting sigma followed by combining Greek ypogegrammeni (0345), Greek upper-case alpha (0391)");
    }
  },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
