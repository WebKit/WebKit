class A1 {
    static {
        var B;
        function C() { }
    }
}

class A2 {
    static {
        {
            var B;
            function C() { }
        }
    }
}

class A3 {
    static {
        var B;
        function B() { }
    }
}

