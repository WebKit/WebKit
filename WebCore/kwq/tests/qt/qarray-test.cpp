#include <iostream>

#include <qarray.h>

int main() {

    QArray<char> a0 = QArray<char>(10);
    QArray<char> a1 = QArray<char>(10);
    QArray<char> a2 = QArray<char>(10);
    QArray<char> a3;

    char c = 'a';
    for (int i = 0; i < 10; i++) {
        a0[i] = c + i;
    }

    a1 = a0;
    cout << "a0: " << a0 << endl;
    cout << "a1: " << a1 << endl;
    cout << "a2: " << a2 << endl;
    cout << "a3: " << a3 << endl;

    cout << "a0 == a1: " << (a0 == a1) << endl;
    cout << "a0 == a2: " << (a0 == a2) << endl;

    cout << "a2 is null: " << (a2.isNull()) << endl;
    cout << "a2 is empty: " << (a2.isEmpty()) << endl;

    cout << "a3 is null: " << (a3.isNull()) << endl;
    cout << "a3 is empty: " << (a3.isEmpty()) << endl;

    return 0;
}
