declare function $ERROR(text: string): void;

// Proposal: regexp-match-indices
interface RegExpExecArray {
    indices: RegExpIndicesArray;
}

interface RegExpMatchArray {
    indices: RegExpIndicesArray;
}

interface RegExpIndicesArray extends Array<[number, number]> {
    groups?: { [group: string]: [number, number] };
}
