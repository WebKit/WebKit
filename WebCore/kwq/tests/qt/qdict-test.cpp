#include <iostream>

#include <qstring.h>
#include <qdict.h>

int main() {

    QDict<int> d0;
    QDict<int> d1;

    int v1 = 1;
    int v2 = 2;
    int v3 = 3;
    int v4 = 4;
    int v5 = 5;

    d1.insert("one", &v1);
    d1.insert("two", &v2);
    d1.insert("three", &v3);
    d1.insert("four", &v4);
    d1.insert("five", &v5);

    cout << d1 << endl;

    cout << "item keyed to 'one': " << *(d1.find("one")) << endl;

    d1.remove("three");
    cout << "remove item keyed to 'three': " << d1 << endl;

    cout << "d0 is empty: " << d0.isEmpty() << endl;
    cout << "d1 is empty: " << d1.isEmpty() << endl;

    return 0;
}
