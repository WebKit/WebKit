
const drinks = {
    Cocoa: 'Cocoa',
    inner: {
        current: [ 'Matcha' ]
    },
    hello: 'Cappuccino'
};

export const {
    Cocoa,
    inner: {
        current: [ Matcha ]
    },
    hello: Cappuccino
} = drinks;
