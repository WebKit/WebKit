#include <iostream>

#include <qstack.h>

int main() {

    QStack<int> s0;
    QStack<int> s1 = QStack<int>();

    int i0 = 0;
    int i1 = 1;
    int i2 = 2;
    int i3 = 3;
    int i4 = 4;

    cout << s0 << endl;

    s0.push(&i0);
    s0.push(&i1);
    s0.push(&i2);
    s0.push(&i3);
    s0.push(&i4);

    cout << "push test: " << endl;
    cout << s0 << endl;
    
    s1 = s0;
    cout << "pop test: " << endl;
    int count = s1.count();
    for (int i = 0; i < count; i++) {
        cout << *(s1.pop()) << endl;
    }

    s1 = s0;
    cout << s1 << endl;
    cout << "clear test: " << endl;
    s1.clear();
    cout << s1 << endl;

    return 0;
}
