// This benchmark tests IntlListFormat.formatToParts which creates many {type, value} objects
// Longer lists generate more part objects, maximizing the optimization benefit
function test() {
    const lf = new Intl.ListFormat('en', { style: 'long', type: 'conjunction' });

    const lists = [
        ['Apple', 'Banana', 'Cherry', 'Date', 'Elderberry'],
        ['Red', 'Orange', 'Yellow', 'Green', 'Blue', 'Indigo', 'Violet'],
        ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday'],
        ['January', 'February', 'March', 'April', 'May', 'June'],
        ['Dog', 'Cat', 'Bird', 'Fish', 'Rabbit', 'Hamster', 'Turtle', 'Snake'],
        ['One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'],
    ];

    let count = 0;
    for (let i = 0; i < 1e4; i++) {
        for (const list of lists) {
            const parts = lf.formatToParts(list);
            count += parts.length;
        }
    }
    return count;
}

const result = test();
if (result !== 760000)
    throw new Error("Bad result: " + result);
