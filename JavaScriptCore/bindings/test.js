myInterface.logMessage ("starting test");

myInterface.logMessage ("myInterface.getInt() = " + myInterface.getInt());

myInterface.logMessage ("myInterface.setInt_(666) = " + myInterface.setInt_(666));
myInterface.logMessage ("myInterface.getInt() = " + myInterface.getInt());
myInterface.logMessage ("myInterface.myInt = " + myInterface.myInt);
myInterface.logMessage ("setting myInterface.myInt = 777");
myInterface.myInt = 777;
myInterface.logMessage ("myInterface.myInt = " + myInterface.myInt);
myInterface.logMessage ("myInterface.getMySecondInterface().doubleValue = " + myInterface.getMySecondInterface().doubleValue);
myInterface.logMessage ("myInterface.getMySecondInterface() = " + myInterface.getMySecondInterface());
