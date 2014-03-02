package org.msg;

public class TestMessenger implements Messenger{

    public void dispatch(String msg){
       if(msg == null || msg.isEmpty()) {throw new java.lang.NullPointerException("empty/null msg provided!");}
       System.out.println("say hello with msg:"+msg);
    }

    public static void main(String args[]){  new TestMessenger().dispatch("hi");  }
}
