# Python Sample
def factorial(n):
    """
    This function calculates the factorial of a non-negative integer.
    """
    if n == 0:
        return 1
    else:
        return n * factorial(n-1)

class Greeter:
    def __init__(self, name):
        self.name = name

    def greet(self):
        print(f"Hello, {self.name}!")

# Example usage
num = 5
print(f"The factorial of {num} is {factorial(num)}")

greeter = Greeter("World")
greeter.greet()
