### Compilation failed:

error: 16: potential recursion (function call cycle) not allowed:
	void f_one(int n)
	void f_two(int n)
	void f_three(int n)
	void f_one(int n)
void f_one(int n) {
                  ^...
1 error
