#include <iostream>

#include <qstring.h>
#include <qstringlist.h>

int main() {

    QStringList p0;

    p0.append("zero");
    p0.append("one");
    p0.append("two");
    p0.append("three");
    p0.append("four");
    p0.append("five");
        
    cout << p0 << endl;
    cout << p0[3] << endl;
    p0 += "six";
    p0 += "seven";
    cout << p0 << endl;

    QValueListIterator<QString> it = p0.find("three");
    while (it != p0.end()) {
        cout << *it;
        if (++it != p0.end()) {
            cout << ", ";
        }
    }
    cout << endl;

    p0.sort();
    cout << p0 << endl;

    return 0;
}
