class Address {
    constructor(i) {
        this.street = "Street " + i;
        this.city = "City";
        this.zip = 10000 + i;
    }
}

class User {
    constructor(i) {
        this.id = i;
        this.name = "user" + i;
        this.email = "user" + i + "@example.com";
        this.active = (i & 1) === 0;
        this.address = new Address(i);
        this.tags = ["a", "b", "c"];
    }
    get displayName() { return this.name.toUpperCase(); }
    greet() { return "hi " + this.name; }
}

class Admin extends User {
    constructor(i) {
        super(i);
        this.level = i % 5;
    }
}

let users = [];
for (let i = 0; i < 40; ++i)
    users.push((i % 4) === 0 ? new Admin(i) : new User(i));
let payload = { ok: true, count: users.length, data: users };

let result;
for (let i = 0; i < 2e4; ++i)
    result = JSON.stringify(payload);
if (result.length !== JSON.stringify(JSON.parse(result)).length)
    throw new Error("bad");
