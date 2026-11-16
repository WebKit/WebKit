// This algorithm runs in O(2^N).
function computeSubsets(anArray)
{
    function compareByOriginalArrayOrder(a, b) {
        if (a.length < b.length)
            return -1;
        if (a.length > b.length)
            return 1;
        for (let i = 0; i < a.length; ++i) {
            let rankA = anArray.indexOf(a[i]);
            let rankB = anArray.indexOf(b[i]);
            if (rankA < rankB)
                return -1;
            if (rankA > rankB)
                return 1;
        }
        return 0;
    }
    let result = [];
    const numberOfNonEmptyPermutations = (1 << anArray.length) - 1;
    // For each ordinal 1, 2, ... numberOfNonEmptyPermutations we look at its binary representation
    // and generate a permutation that consists of the entries in anArray at the indices where there
    // is a one bit in the binary representation. For example, suppose anArray = ["metaKey", "altKey", "ctrlKey"].
    // To generate the 5th permutation we look at the binary representation of i = 5 => 0b101. And
    // compute the permutation to be [ anArray[0], anArray[2] ] = [ "metaKey", "ctrlKey" ] because
    // the 0th and 2nd bits are ones in the binary representation.
    for (let i = 1; i <= numberOfNonEmptyPermutations; ++i) {
        let temp = [];
        for (let bitmask = i, j = 0; bitmask; bitmask = Math.floor(bitmask / 2), ++j) {
            if (bitmask % 2)
                temp.push(anArray[j]);
        }
        result.push(temp);
    }
    return result.sort(compareByOriginalArrayOrder);
}
