#include <iostream>

#include <qarray.h>

int main() {

    QArray<char> a0 = QArray<char>(10);
    QArray<char> a1 = QArray<char>(10);
    QArray<char> a2 = QArray<char>(10);
    QArray<char> a3;
    char *ch;
    
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
    cout << "a0 != a1: " << (a0 != a1) << endl;
    cout << "a0 != a2: " << (a0 != a2) << endl;
    
    cout << "a2 is null: " << (a2.isNull()) << endl;
    cout << "a2 is empty: " << (a2.isEmpty()) << endl;

    cout << "a3 is null: " << (a3.isNull()) << endl;
    cout << "a3 is empty: " << (a3.isEmpty()) << endl;

    cout << "a0 count: " << (a0.count()) << endl;
    
    ch = a0.data();
    ch[10] = '\0';
    cout << "a0 data: " << ch << endl;
    
    a0.resize(15);
    for (int i = 10; i < 15; i++) {
        a0[i] = c + i;
    }
    cout << "a0 resize: " << a0 << endl;  
    
    QArray<char> a4 = QArray<char>(10);
    a4.fill('c');
    cout << "a4 fill: " << a4 << endl;
    
    //cout << "a0 find 'j': " << a0.find('j') << endl;
    
    //cout << "a0 nrefs: " << a0.nrefs() << endl;
    
    //QArray<char> a5 = QArray<char>(10);
    //a5 = a0.copy();
    //cout << "a5 copy a0: " << a5 << endl;
    
    //QArray<char> a6 = QArray<char>(10);
    //a6 = a0.assign(a0); 			// hmm. Am I doing this right?
    //cout << "a6 assign a0: " << a6 << endl;
    
    //QArray<char> a7 = QArray<char>(10);
    //for (int i = 9; i >= 0; i--) {
    //    a7[i] = 'j' - i;
    //}
    
    //cout << "a7 unsorted: " << a7 << endl;
    //a7.sort();
    //cout << "a7 sorted: " << a7 << endl;
    
    
    char ch2;
    ch2 = a0.at(4);
    cout << "a0 at 4: " << ch2 << endl;
    
    char *ch3;
    ch3 = a4.data();
    a0 = a0.duplicate(ch3, 10);
    cout << "a0 duplicate from a4: " << a0 << endl;
    
    return 0;
}
