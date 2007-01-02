//mapApp that handles window resizing
var myMapApp = new mapApp(false,undefined);
var fruitSorts;
var myFruitsResult = new fruitsResult();

//global selectionLists
var selFruits;
var selFlowers;
var selCommunitiesAargau;
var selRoses;
var selnumbers1;
var selnumbers2;

function initMap(evt) {
    //styling parameters
    var selBoxCellHeight = 16;
    var selBoxTextpadding = 3;
    var selBoxtextStyles = {"font-family":"Arial,Helvetica","font-size":11,"fill":"dimgray"};
    var selBoxStyles = {"stroke":"dimgray","stroke-width":1,"fill":"white"};
    var selBoxScrollbarStyles = {"stroke":"dimgray","stroke-width":1,"fill":"whitesmoke"};
    var selBoxSmallrectStyles = {"stroke":"dimgray","stroke-width":1,"fill":"lightgray"};
    var selBoxHighlightStyles = {"fill":"dimgray","fill-opacity":0.3};
    var selBoxTriangleStyles = {"fill":"dimgray"};    
    
    //arrays for selectionList data
    var fruits = ["Oranges","Apples","Bananas","Pears"];
    fruitSorts = {"Oranges":["Navel"],"Apples":["Fuji","Granny Smith","Pink Lady"],"Bananas":["Chiquita","Havel"],"Pears":["Alexander","Gute Luise"]};
    var flowers = new Array("Acacia","Acanthus","Amaranth","Anthericum","Arum","Ash","Aspen","Aster","Balm","Barbery","Basil","Bellflower","Bindweed","Bird cherry-tree","Black thorn","Bladder-senna","Bluebottle","Borage","Box");
    var roses = new Array("Butterscotch","Ci Peace","Impatient","Lady Hillingdon","Lavaglut","Mission Bells","Sexy Rexy","Souvenir de Pierre Notting","Sunflare","Whisky Mac","Whisper Floribunda");
    var communitiesAarau = new Array("Aarau","Aarburg","Abtwil","Ammerswil","Aristau","Arni (AG)","Attelwil","Auenstein","Auw","Baden","Baldingen","Beinwil (Freiamt)","Beinwil am See","Bellikon","Benzenschwil","Bergdietikon","Berikon","Besenbueren","Bettwil","Biberstein","Birmenstorf (AG)","Birr","Birrhard","Birrwil","Boniswil","Boswil","Bottenwil","Bremgarten (AG)","Brittnau","Brugg","Brunegg","Buchs (AG)","Burg (AG)","Buttwil","Boebikon","Boettstein","Boezen","Buenzen","Buettikon","Densbueren","Dietwil","Dintikon","Dottikon","Doettingen","Duerrenaesch","Effingen","Eggenwil","Egliswil","Eiken","Elfingen","Endingen","Ennetbaden","Erlinsbach","Etzgen","Fahrwangen","Fischbach-Goeslikon","Fisibach","Fislisbach","Freienwil","Frick","Full-Reuenthal","Gallenkirch","Gansingen","Gebenstorf","Geltwil","Gipf-Oberfrick","Gontenschwil","Graenichen","Habsburg","Hallwil","Hausen bei Brugg","Hellikon","Hendschiken","Hermetschwil-Staffeln","Herznach","Hilfikon","Hirschthal","Holderbank (AG)","Holziken","Hornussen","Hottwil","Hunzenschwil","Haegglingen","Islisberg","Ittenthal","Jonen","Kaiseraugst","Kaiserstuhl","Kaisten","Kallern","Killwangen","Kirchleerau","Klingnau","Koblenz","Koelliken","Kuenten","Kuettigen","Laufenburg","Leibstadt","Leimbach (AG)","Lengnau (AG)","Lenzburg","Leuggern","Leutwil","Linn","Lupfig","Magden","Mandach","Meisterschwanden","Mellikon","Mellingen","Menziken","Merenschwand","Mettau","Moosleerau","Muhen","Mumpf","Murgenthal","Muri (AG)","Maegenwil","Moehlin","Moenthal","Moeriken-Wildegg","Muehlau","Muehlethal","Muelligen","Muenchwilen (AG)","Neuenhof","Niederlenz","Niederrohrdorf","Niederwil (AG)","Oberboezberg","Oberehrendingen","Oberentfelden","Oberflachs","Oberhof","Oberhofen (AG)","Oberkulm","Oberlunkhofen","Obermumpf","Oberrohrdorf","Oberrueti","Obersiggenthal","Oberwil-Lieli","Oeschgen","Oftringen","Olsberg","Othmarsingen","Reinach (AG)","Reitnau","Rekingen (AG)","Remetschwil","Remigen","Rheinfelden","Rietheim","Riniken","Rohr (AG)","Rothrist","Rottenschwil","Rudolfstetten-Friedlisberg","Rupperswil","Ruefenach","Ruemikon","Safenwil","Sarmenstorf","Schafisheim","Scherz","Schinznach Bad","Schinznach Dorf","Schlossrued","Schmiedrued","Schneisingen","Schupfart","Schwaderloch","Schoeftland","Seengen","Seon","Siglistorf","Sins","Sisseln","Spreitenbach","Staffelbach","Staufen","Stein (AG)","Stetten (AG)","Stilli","Strengelbach","Suhr","Sulz (AG)","Tegerfelden","Teufenthal (AG)","Thalheim (AG)","Turgi","Taegerig","Ueken","Uerkheim","Uezwil","Umiken","Unterboezberg","Unterehrendingen","Unterendingen","Unterentfelden","Unterkulm","Unterlunkhofen","Untersiggenthal","Veltheim (AG)","Villigen","Villmergen","Villnachern","Vordemwald","Wallbach","Waltenschwil","Wegenstetten","Wettingen","Widen","Wil (AG)","Wiliberg","Windisch","Wislikofen","Wittnau","Wohlen (AG)","Wohlenschwil","Woelflinswil","Wuerenlingen","Wuerenlos","Zeihen","Zeiningen","Zetzwil","Zofingen","Zufikon","Zurzach","Zuzgen");
    
    //usage: var newSelList = new selectionList(groupId,elementsArray,width,xOffset,yOffset,cellHeight,textPadding,heightNrElements,textStyles,boxStyles,scrollbarStyles,smallrectStyles,highlightStyles,triangleStyles,preSelect,openAbove,putOnTopOfParent,functionToCall);
    //create an empty group with the id as specified above in parameter 'groupId'
    selFruits = new selectionList("fruits","fruits",fruits,170,50,50,selBoxCellHeight,selBoxTextpadding,7,selBoxtextStyles,selBoxStyles,selBoxScrollbarStyles,selBoxSmallrectStyles,selBoxHighlightStyles,selBoxTriangleStyles,3,false,true,myFruitsResult);
    selFruits.sortList("asc");
    selRoses = new selectionList("roses","roses",roses,170,50,100,selBoxCellHeight,selBoxTextpadding,4,selBoxtextStyles,selBoxStyles,selBoxScrollbarStyles,selBoxSmallrectStyles,selBoxHighlightStyles,selBoxTriangleStyles,0,false,true,showRoses);
    selFlowers = new selectionList("flowers","flowers",flowers,170,50,720,selBoxCellHeight,selBoxTextpadding,5,selBoxtextStyles,selBoxStyles,selBoxScrollbarStyles,selBoxSmallrectStyles,selBoxHighlightStyles,selBoxTriangleStyles,0,true,true,undefined);
    selCommunitiesAargau = new selectionList("communitiesAarau","communitiesAarau",communitiesAarau,150,50,200,selBoxCellHeight,selBoxTextpadding,10,selBoxtextStyles,selBoxStyles,selBoxScrollbarStyles,selBoxSmallrectStyles,selBoxHighlightStyles,selBoxTriangleStyles,0,false,true,undefined);
    var numbers1 = new Array("1","2","3","4","5","6","7","8","9");
    var newArray = generateRandomArray("1");
    selnumbers2 = new selectionList("numbers2","selnumbers",newArray,50,700,100,selBoxCellHeight,selBoxTextpadding,10,selBoxtextStyles,selBoxStyles,selBoxScrollbarStyles,selBoxSmallrectStyles,selBoxHighlightStyles,selBoxTriangleStyles,0,false,true,undefined);    
    selnumbers1 = new selectionList("numbers1","selnumbers",numbers1,30,700,50,selBoxCellHeight,selBoxTextpadding,10,selBoxtextStyles,selBoxStyles,selBoxScrollbarStyles,selBoxSmallrectStyles,selBoxHighlightStyles,selBoxTriangleStyles,0,false,true,updateNumbers2);
}

function removeSelFlowers() {
    if (selFlowers.exists == true) {
        selFlowers.removeSelectionList();
    }
    else {
        alert('selectionList already removed');
    }
}

function fruitsResult() { }

fruitsResult.prototype.getSelectionListVal = function(selBoxName,fruitNr,arrayVal) {
    alert("You selected Fruit Nr. "+(fruitNr+1)+": "+arrayVal+". Yum!\nAvailable Sorts are: "+fruitSorts[arrayVal].join(", "));
}

function showRoses(selBoxName,roseNr,arrayVal) {
    document.getElementById("rosename").firstChild.nodeValue = arrayVal;
    roseName = "images/" + arrayVal.toLowerCase().replace(/\s/g,"_") + ".jpg";
    document.getElementById("roseimage").setAttributeNS(xlinkNS,"xlink:href",roseName);
}

function toggleAargauOpenMode() {
    if (selCommunitiesAargau.openAbove) {
        selCommunitiesAargau.openAbove = false;
    }
    else {
        selCommunitiesAargau.openAbove = true;
    }
}

function generateRandomArray(baseVal) {
    var newArray = new Array();
    for(var i=0;i<10;i++) {

// THIS LINE IS OUTCOMMENTED TO GET CONSISTENT RESULTS WHEN LAYOUT TESTING IN WEBKIT!
//        newArray[i] = baseVal + (Math.round(Math.random()*1000));

                newArray[i] = baseVal;
    }
    return newArray;
}

function updateNumbers2(selBoxName,index,value) {
    var newArray = generateRandomArray(value);
    selnumbers2.elementsArray = newArray;
    selnumbers2.selectElementByPosition(0,false);
}
