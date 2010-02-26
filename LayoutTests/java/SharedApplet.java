import java.applet.Applet;
import java.lang.reflect.*;
import java.util.*;
import netscape.javascript.*;

class NonPublicClass {
    NonPublicClass() {
    }
    public Object arrayField[] = { 5 };
}

public class SharedApplet extends Applet
{
    public String objectToString(Object obj) {
        return obj.toString();
    }

    public String[] stringArray() {
        return new String[] { "One", "Two", "Three" };
    }

    public List<String> stringList() {
        List<String> result = new ArrayList<String>();
        result.add("One");
        result.add("Two");
        result.add("Three");
        return result;
    }

    public Object getSelf() {
        return this;
    }

    public NonPublicClass getObjectOfNonPublicClass() {
        return new NonPublicClass();
    }

    public Object testGetProperty(JSObject obj, String propertyName) {
        return obj.getMember(propertyName);
    }

    public Object testGetMember(JSObject obj, String memberName) {
        return obj.getMember(memberName);
    }

    public void remember(Object obj) {
        rememberedObject = obj;
    }

    public Object getAndForgetRememberedObject() {
        Object result = rememberedObject;
        rememberedObject = null;
        return result;
    }

    private Object rememberedObject;
}
