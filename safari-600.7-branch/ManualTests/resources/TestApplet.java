import java.applet.Applet;

public class TestApplet extends Applet {
    public static int MAGIC_NUMBER = 1;
    public int field;
    
    public void init()
    {
        field = MAGIC_NUMBER;
    }
    
    public int method()
    {
        return MAGIC_NUMBER;
    }
}