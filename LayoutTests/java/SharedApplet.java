import java.applet.Applet;
import java.util.*;

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
}
