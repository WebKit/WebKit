#include <iostream>

#include <qstack.h>

int main() {

    QStack<int> s0;
    QStack<int> s1 = QStack<int>();
    QStack<int> s2;
    
    int i0 = 0;
    int i1 = 1;
    int i2 = 2;
    int i3 = 3;
    int i4 = 4;

    cout << "s0: " << s0 << endl;

    s0.push(&i0);
    s0.push(&i1);
    s0.push(&i2);
    s0.push(&i3);
    s0.push(&i4);
    cout << "s0 push: " << s0 << endl;
    
    s1 = s0;
    cout << "s1 = s0: " << s1 << endl;
    cout << "s1 count: " << s1.count() << endl;
    
    int count = s1.count();
    for (int i = 0; i < count; i++) {
        cout << "s1 pop: "<< *(s1.pop()) << endl;
    }
    cout << "s1: " << s1 << endl;
    
    cout << "s0 isEmpty: " << s0.isEmpty() << endl;
    cout << "s1 isEmpty: " << s1.isEmpty() << endl;
    
    s0.clear();  // not needed for KWQ?
    cout << "s0 clear: "<< s0 << endl;

    return 0;
}
