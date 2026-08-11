//mapApp that handles window resizing
var myMapApp = new mapApp(false,undefined);

//global selectionLists
var comboFruits;
var comboFlowers;

function initMap(evt) {
    //first a few styling parameters:
    var comboBoxCellHeight = 16;
    var comboBoxTextpadding = 3;
    var comboBoxtextStyles = {"font-family":"Arial,Helvetica","font-size":11,"fill":"dimgray"};
    var comboBoxStyles = {"stroke":"dimgray","stroke-width":1,"fill":"white"};
    var comboBoxScrollbarStyles = {"stroke":"dimgray","stroke-width":1,"fill":"whitesmoke"};
    var comboBoxSmallrectStyles = {"stroke":"dimgray","stroke-width":1,"fill":"lightgray"};
    var comboBoxHighlightStyles = {"fill":"dimgray","fill-opacity":0.3};
    var comboBoxTriangleStyles = {"fill":"dimgray"};
    //arrays for selectionList data
    var fruitsGerman = [{key:"Orangen",value:false},{key:"Äpfel",value:false},{key:"Bananen",value:true},{key:"Birnen",value:false}];
    var flowers = new Array({key:"Acacia",value:false},{key:"Acanthus",value:false},{key:"Amaranth",value:false},{key:"Anthericum",value:false},{key:"Arum",value:false},{key:"Ash",value:true},{key:"Aspen",value:false},{key:"Aster",value:false},{key:"Balm",value:false},{key:"Barbery",value:false},{key:"Basil",value:false},{key:"Bellflower",value:true},{key:"Bindweed",value:false},{key:"Bird cherry-tree",value:false},{key:"Black thorn",value:false},{key:"Bladder-senna",value:false},{key:"Bluebottle",value:false},{key:"Borage",value:false},{key:"Box",value:false});
    var roses = new Array({key:"Butterscotch",value:false},{key:"Ci Peace",value:false},{key:"Impatient",value:false},{key:"Lady Hillingdon",value:false},{key:"Lavaglut",value:false},{key:"Mission Bells",value:false},{key:"Sexy Rexy",value:false},{key:"Souvenir de Pierre Notting",value:false},{key:"Sunflare",value:false},{key:"Whisky Mac",value:false},{key:"Whisper Floribunda",value:false});
    var communitiesAarau = new Array({key:"Aarau",value:false},{key:"Aarburg",value:false},{key:"Abtwil",value:true},{key:"Ammerswil",value:false},{key:"Aristau",value:false},{key:"Arni (AG)",value:false},{key:"Attelwil",value:false},{key:"Auenstein",value:false},{key:"Auw",value:false},{key:"Baden",value:false},{key:"Baldingen",value:false},{key:"Beinwil (Freiamt)",value:false},{key:"Beinwil am See",value:false},{key:"Bellikon",value:false},{key:"Benzenschwil",value:false},{key:"Bergdietikon",value:false},{key:"Berikon",value:false},{key:"Besenbüren",value:false},{key:"Bettwil",value:false},{key:"Biberstein",value:false},{key:"Birmenstorf (AG)",value:false},{key:"Birr",value:false},{key:"Birrhard",value:false},{key:"Birrwil",value:false},{key:"Boniswil",value:false},{key:"Boswil",value:false},{key:"Bottenwil",value:false},{key:"Bremgarten (AG)",value:false},{key:"Brittnau",value:false},{key:"Brugg",value:false},{key:"Brunegg",value:false},{key:"Buchs (AG)",value:false},{key:"Burg (AG)",value:false},{key:"Buttwil",value:false},{key:"Böbikon",value:false},{key:"Böttstein",value:false},{key:"Bözen",value:false},{key:"Bünzen",value:false},{key:"Buettikon",value:false},{key:"Densbueren",value:false},{key:"Dietwil",value:false},{key:"Dintikon",value:false},{key:"Dottikon",value:false},{key:"Döttingen",value:false},{key:"Dürrenäsch",value:false},{key:"Effingen",value:false},{key:"Eggenwil",value:false},{key:"Egliswil",value:false},{key:"Eiken",value:false},{key:"Elfingen",value:false},{key:"Endingen",value:false},{key:"Ennetbaden",value:false},{key:"Erlinsbach",value:false},{key:"Etzgen",value:false},{key:"Fahrwangen",value:false},{key:"Fischbach-Goeslikon",value:false},{key:"Fisibach",value:false},{key:"Fislisbach",value:false},{key:"Freienwil",value:false},{key:"Frick",value:false},{key:"Full-Reuenthal",value:false},{key:"Gallenkirch",value:false},{key:"Gansingen",value:false},{key:"Gebenstorf",value:false},{key:"Geltwil",value:false},{key:"Gipf-Oberfrick",value:false},{key:"Gontenschwil",value:false},{key:"Graenichen",value:false},{key:"Habsburg",value:false},{key:"Hallwil",value:false},{key:"Hausen bei Brugg",value:false},{key:"Hellikon",value:false},{key:"Hendschiken",value:false},{key:"Hermetschwil-Staffeln",value:false},{key:"Herznach",value:false},{key:"Hilfikon",value:false},{key:"Hirschthal",value:false},{key:"Holderbank (AG)",value:false},{key:"Holziken",value:false},{key:"Hornussen",value:true},{key:"Hottwil",value:false},{key:"Hunzenschwil",value:false},{key:"Haegglingen",value:false},{key:"Islisberg",value:false},{key:"Ittenthal",value:false},{key:"Jonen",value:false},{key:"Kaiseraugst",value:false},{key:"Kaiserstuhl",value:false},{key:"Kaisten",value:false},{key:"Kallern",value:false},{key:"Killwangen",value:false},{key:"Kirchleerau",value:false},{key:"Klingnau",value:true},{key:"Koblenz",value:true},{key:"Koelliken",value:false},{key:"Kuenten",value:false},{key:"Kuettigen",value:false},{key:"Laufenburg",value:false},{key:"Leibstadt",value:false},{key:"Leimbach (AG)",value:false},{key:"Lengnau (AG)",value:false},{key:"Lenzburg",value:false},{key:"Leuggern",value:false},{key:"Leutwil",value:false},{key:"Linn",value:false},{key:"Lupfig",value:false},{key:"Magden",value:false},{key:"Mandach",value:false},{key:"Meisterschwanden",value:false},{key:"Mellikon",value:false},{key:"Mellingen",value:false},{key:"Menziken",value:false},{key:"Merenschwand",value:false},{key:"Mettau",value:false},{key:"Moosleerau",value:false},{key:"Muhen",value:false},{key:"Mumpf",value:false},{key:"Murgenthal",value:false},{key:"Muri (AG)",value:false},{key:"Maegenwil",value:false},{key:"Moehlin",value:false},{key:"Moenthal",value:false},{key:"Moeriken-Wildegg",value:false},{key:"Muehlau",value:false},{key:"Mühlethal",value:false},{key:"Mülligen",value:false},{key:"Muenchwilen (AG)",value:false},{key:"Neuenhof",value:false},{key:"Niederlenz",value:false},{key:"Niederrohrdorf",value:false},{key:"Niederwil (AG)",value:false},{key:"Oberbözberg",value:false},{key:"Oberehrendingen",value:false},{key:"Oberentfelden",value:false},{key:"Oberflachs",value:false},{key:"Oberhof",value:false},{key:"Oberhofen (AG)",value:false},{key:"Oberkulm",value:false},{key:"Oberlunkhofen",value:false},{key:"Obermumpf",value:false},{key:"Oberrohrdorf",value:false},{key:"Oberrueti",value:false},{key:"Obersiggenthal",value:false},{key:"Oberwil-Lieli",value:false},{key:"Oeschgen",value:false},{key:"Oftringen",value:true},{key:"Olsberg",value:false},{key:"Othmarsingen",value:false},{key:"Reinach (AG)",value:false},{key:"Reitnau",value:false},{key:"Rekingen (AG)",value:false},{key:"Remetschwil",value:false},{key:"Remigen",value:false},{key:"Rheinfelden",value:false},{key:"Rietheim",value:false},{key:"Riniken",value:false},{key:"Rohr (AG)",value:false},{key:"Rothrist",value:false},{key:"Rottenschwil",value:false},{key:"Rudolfstetten-Friedlisberg",value:false},{key:"Rupperswil",value:false},{key:"Ruefenach",value:false},{key:"Ruemikon",value:false},{key:"Safenwil",value:false},{key:"Sarmenstorf",value:false},{key:"Schafisheim",value:false},{key:"Scherz",value:false},{key:"Schinznach Bad",value:false},{key:"Schinznach Dorf",value:false},{key:"Schlossrued",value:false},{key:"Schmiedrued",value:false},{key:"Schneisingen",value:false},{key:"Schupfart",value:false},{key:"Schwaderloch",value:false},{key:"Schöftland",value:false},{key:"Seengen",value:false},{key:"Seon",value:false},{key:"Siglistorf",value:false},{key:"Sins",value:false},{key:"Sisseln",value:false},{key:"Spreitenbach",value:false},{key:"Staffelbach",value:false},{key:"Staufen",value:false},{key:"Stein (AG)",value:false},{key:"Stetten (AG)",value:false},{key:"Stilli",value:false},{key:"Strengelbach",value:false},{key:"Suhr",value:false},{key:"Sulz (AG)",value:false},{key:"Tegerfelden",value:false},{key:"Teufenthal (AG)",value:false},{key:"Thalheim (AG)",value:false},{key:"Turgi",value:false},{key:"Tägerig",value:false},{key:"Ueken",value:false},{key:"Uerkheim",value:false},{key:"Uezwil",value:false},{key:"Umiken",value:false},{key:"Unterboezberg",value:false},{key:"Unterehrendingen",value:false},{key:"Unterendingen",value:false},{key:"Unterentfelden",value:false},{key:"Unterkulm",value:false},{key:"Unterlunkhofen",value:false},{key:"Untersiggenthal",value:false},{key:"Veltheim (AG)",value:false},{key:"Villigen",value:false},{key:"Villmergen",value:false},{key:"Villnachern",value:false},{key:"Vordemwald",value:false},{key:"Wallbach",value:false},{key:"Waltenschwil",value:false},{key:"Wegenstetten",value:false},{key:"Wettingen",value:false},{key:"Widen",value:false},{key:"Wil (AG)",value:false},{key:"Wiliberg",value:false},{key:"Windisch",value:false},{key:"Wislikofen",value:false},{key:"Wittnau",value:false},{key:"Wohlen (AG)",value:false},{key:"Wohlenschwil",value:false},{key:"Wölflinswil",value:false},{key:"Würenlingen",value:false},{key:"Würenlos",value:false},{key:"Zeihen",value:false},{key:"Zeiningen",value:false},{key:"Zetzwil",value:false},{key:"Zofingen",value:false},{key:"Zufikon",value:false},{key:"Zurzach",value:false},{key:"Zuzgen",value:false});
    //usage: var newSelList = new selectionList(groupName,elementsArray,width,xOffset,yOffset,heightNrElements,preSelect,functionToCall);
    //create an empty group with the id as specified above in parameter 'groupName'
    comboFruits = new combobox("fruits","fruits",fruitsGerman,170,50,50,comboBoxCellHeight,comboBoxTextpadding,7,false,0,comboBoxtextStyles,comboBoxStyles,comboBoxScrollbarStyles,comboBoxSmallrectStyles,comboBoxHighlightStyles,comboBoxTriangleStyles,undefined);
    //selFruits.sortList("asc");
    var comboRoses = new combobox("roses","roses",roses,170,50,300,comboBoxCellHeight,comboBoxTextpadding,5,true,50,comboBoxtextStyles,comboBoxStyles,comboBoxScrollbarStyles,comboBoxSmallrectStyles,comboBoxHighlightStyles,comboBoxTriangleStyles,showRoses);
    comboFlowers = new combobox("flowers","flowers",flowers,170,50,550,comboBoxCellHeight,comboBoxTextpadding,8,true,2,comboBoxtextStyles,comboBoxStyles,comboBoxScrollbarStyles,comboBoxSmallrectStyles,comboBoxHighlightStyles,comboBoxTriangleStyles,undefined);
    var comboCommunitiesAargau = new combobox("communitiesAarau","communitiesAarau",communitiesAarau,150,50,200,comboBoxCellHeight,comboBoxTextpadding,10,true,25,comboBoxtextStyles,comboBoxStyles,comboBoxScrollbarStyles,comboBoxSmallrectStyles,comboBoxHighlightStyles,comboBoxTriangleStyles,undefined);
    //select additional elements by name
    //var additionalElements = new Array({key:"Boswil",value:true},{key:"Brugg",value:true},{key:"Brunegg",value:true},{key:"Buttwil",value:false});
    //comboCommunitiesAargau.selectElementsByName(additionalElements,false);
    //select additional elements by index
    //var additionalElements = new Array({index:0,value:true},{index:2,value:true},{index:4,value:true},{index:6,value:false});
    //comboCommunitiesAargau.selectElementsByPosition(additionalElements,false);
    //sort communities desc
    //comboFruits.sortList("asc");
    //get current selections
    //alert(comboCommunitiesAargau.getCurrentSelections().join(","));
    //get current selections index
    //alert(comboCommunitiesAargau.getCurrentSelectionsIndex().join(","));
    //remove combobox
    //comboCommunitiesAargau.removeCombobox();
    //delete elements
    //var deleteElements = new Array("Boswil","Brugg","Brunegg","Buttwil");
    //comboCommunitiesAargau.deleteElementsByName(deleteElements,false);
    //delete element by position, zero based
    //comboCommunitiesAargau.deleteElementByPosition(1,false);
    //add element at a certain position
    //comboCommunitiesAargau.addElementAtPosition({key:"Götzens",value:true},0,false);
    //add multiple elements alphabetically
    //var addElementsArray = new Array({key:"Zürich",value:true},{key:"Rickenbach (ZH)",value:true},{key:"Innsbruck",value:false});
    //comboCommunitiesAargau.addElementsAlphabetically(addElementsArray,"desc",false);
}

function removeSelFlowers() {
    if (selFlowers.exists == true) {
        selFlowers.removeSelectionList();
    }
    else {
        alert('selectionList already removed');
    }
}

function showRoses(comboboxName,selectedValues,selectedIndizes) {
    var rosesString = selectedValues.join(",");
    if (rosesString.length == 0) {
        rosesString = " ";
    }
    document.getElementById("selectedRoses").firstChild.nodeValue = rosesString;
}
