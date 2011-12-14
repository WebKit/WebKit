import java.applet.Applet;
import java.awt.*;
import java.awt.event.*;


public class ArrayParameterTestApplet
    extends Applet
{
    public void init()
    {
        setLayout(new BorderLayout());
    }


    public void start()
    {
    }


    public void stop()
    {
    }


    public void destroy()
    {
    }


    public void arrayFunction(String [] array) {
        System.out.println("arrayFunction called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
    
    public void booleanFunction(boolean[] array) {
        System.out.println("booleanArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }

    public void byteFunction(byte[] array) {
        System.out.println("byteArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
    
    public void charFunction(char[] array) {
        System.out.println("charArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
        
    public void shortFunction(short[] array) {
        System.out.println("shortArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
        
    public void intFunction(int[] array) {
        System.out.println("intArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
        
    public void longFunction(long[] array) {
        System.out.println("longArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
        
    public void floatFunction(float[] array) {
        System.out.println("floatArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
        
    public void doubleFunction(double[] array) {
        System.out.println("doubleArray called");
        for (int i = 0; i < array.length; i++)
            System.out.println(array[i]);
        }
        
    public void objectFunction(Applet[] array) {
        System.out.println("objectArray called");
        for (int i = 0; i < array.length; i++) 
            System.out.println(array[i]);
        }
}
