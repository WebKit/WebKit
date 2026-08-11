// TypeScript Sample
interface Person {
    firstName: string;
    lastName: string;
}

function greeter(person: Person): string {
    return "Hello, " + person.firstName + " " + person.lastName;
}

let user: Person = { firstName: "Jane", lastName: "User" };

console.log(greeter(user));

class Student {
    fullName: string;
    constructor(public firstName: string, public middleInitial: string, public lastName: string) {
        this.fullName = firstName + " " + middleInitial + " " + lastName;
    }
}

let student = new Student("John", "Q.", "Public");
console.log("Student:", student.fullName);
