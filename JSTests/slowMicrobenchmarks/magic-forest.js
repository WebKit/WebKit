// See http://unriskinsight.blogspot.com/2014/06/fast-functional-goats-lions-and-wolves.html
// Sascha Kratky (kratky@unrisk.com), uni software plus GmbH & MathConsult GmbH
//
// run with Google v8:
// d8 magicForest.js -- 117 155 106
//
// run with Mozilla SpiderMonkey:
// js magicForest.js 117 155 106

function Forest(goats, wolves, lions) {
   this.goats = goats;
   this.wolves = wolves;
   this.lions = lions;
}

Forest.prototype.wolfDevoursGoat = function() {
    if (this.goats > 0 && this.wolves > 0)
        return new Forest(this.goats - 1, this.wolves - 1, this.lions + 1);
    else
        return null;
}

Forest.prototype.lionDevoursGoat = function() {
    if (this.goats > 0 && this.lions > 0)
        return new Forest(this.goats - 1, this.wolves + 1, this.lions - 1);
    else
        return null;
}

Forest.prototype.lionDevoursWolf = function() {
    if (this.lions > 0 && this.wolves > 0)
        return new Forest(this.goats + 1, this.wolves - 1, this.lions - 1);
    else
        return null;
}

Forest.prototype.meal = function() {
    return [this.wolfDevoursGoat(), this.lionDevoursGoat(), this.lionDevoursWolf()].filter(
        function(f) { return f !== null; })
}

Forest.prototype.isStable = function() {
    if (this.goats === 0) return (this.wolves === 0) || (this.lions === 0);
    return (this.wolves === 0) && (this.lions === 0);
}

Forest.prototype.toString = function()
{
    return "[goats=" + this.goats + ", wolves=" + this.wolves + ", lions=" + this.lions + "]";
}

Forest.compare = function(forest1, forest2) {
    var cmp = forest1.goats-forest2.goats;
    if (cmp !== 0) return cmp;
    cmp = forest1.wolves-forest2.wolves;
    if (cmp !== 0) return cmp;
    return forest1.lions-forest2.lions;
}

function meal(forests) {
    var nextForests = [];
    forests.forEach(function(forest) {
        nextForests.push.apply(nextForests, forest.meal())
    });
    // remove duplicates
    return nextForests.sort(Forest.compare).filter(function(forest, index, array) {
        return (index === 0 || Forest.compare(forest, array[index - 1]) !== 0);
    });
}

function devouringPossible(forests) {
    return forests.length > 0 && !forests.some(function(f) { return f.isStable(); });
}

function stableForests(forests) {
    return forests.filter(function(f) { return f.isStable(); });
}

function findStableForests(forest) {
    var forests = [forest];
    while (devouringPossible(forests)) {
        forests = meal(forests);
    }
    return stableForests(forests);
}

function go(g,w,l) {
    var initialForest = new Forest(g, w, l);
    findStableForests(initialForest)/*.forEach(function(f){ console.log(f); })*/
}

function goUI()
{
    d=new Date();
    g=parseInt("217");
    w=parseInt("255");
    l=parseInt("206");
    go(g, w, l);
    // print("Time taken: " + (new Date() - d) + "ms");
}
goUI();
