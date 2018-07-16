TestPage.registerInitializer(() => {

    window.testSuiteFixture1 = class testSuiteFixture1 extends WI.AuditTestSuite
    {
        constructor()
        {
            super("FakeTestSuite1", "FakeTestSuite1");
        }

        static testCaseDescriptors()
        {
            return [
                {
                    name: "fakeTest1",
                    async test(){}
                },
                {
                    name: "fakeTest2",
                    async test()
                    {
                        throw new Error([1, 2, 3, 4]);
                    }
                }
            ];
        }
    }

    window.testSuiteFixture2 = class testSuiteFixture2 extends WI.AuditTestSuite
    {
        constructor()
        {
            super("FakeTestSuite2", "FakeTestSuite2");
        }

        static testCaseDescriptors()
        {
            return [
                {
                    name: "fakeTest2",
                    async test()
                    {
                        return [];
                    }
                }
            ];
        }
    }
  
});
