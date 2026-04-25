#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

class MyClass {
public:
    MyClass(const std::string& name) : name_(name) {}

    void printName() const {
        std::cout << "Name: " << name_ << std::endl;
    }

private:
    std::string name_;
};

template<typename T>
void printVector(const std::vector<T>& vec) {
    for (const auto& item : vec) {
        std::cout << item << " ";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "Hello, C++ World!" << std::endl;

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    printVector(numbers);

    std::vector<std::string> strings = {"apple", "banana", "cherry"};
    printVector(strings);

    std::sort(strings.begin(), strings.end());
    printVector(strings);

    MyClass obj("Test Object");
    obj.printName();

    return 0;
}
