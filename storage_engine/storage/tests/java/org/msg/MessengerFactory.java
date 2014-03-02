package org.msg;
import org.msg.TestMessenger;

public class MessengerFactory{

    public static Messenger getMessenger(String config)
    {
         return new TestMessenger(); //config does not matter as this is for tests.
    }
    public static void destroy(String config){
       if(config == null || config.isEmpty()) {throw new java.lang.NullPointerException("empty/null config provided!");}
       System.out.println("Destroyed messenger");
    }
}
