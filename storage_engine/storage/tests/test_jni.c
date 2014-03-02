#include "../r2d2/ha_jni.h"
#include <stdlib.h>
static JNIEnv* env ;


static int failures = 0;
#define SUCCESS 0
#define FAILED 1


#define succeed(test) printf("\n---------> Successfull Test: %s ..\n",test); return SUCCESS;
#define fail(test) printf("\n ====================="); printf("\n-> Test Failed: %s ..\n",test); failures++; printf("\n =====================\n"); exit(FAILED);

#define running(test) printf("\n----> Running Test: %s ..\n",test);

int test_load_unknown_class(){

     running("test_load_unknown_class");
     if(load_class(env, "fooobar" ) != NULL ){
        fail("test_load_unknown_class");
     }
     succeed("test_load_unknown_class");
}

int test_load_known_class(){

     running("test_load_known_class");
     if(load_class(env, "org/msg/Messenger" ) == NULL ){
        fail("test_load_known_class");
     }
     succeed("test_load_known_class");
}

int test_get_unknown_static_method(){
//    get_static_method(JNIEnv* env, jclass class_object,char * class_name ,char * static_method_name, char * static_method_signature )
    
    running("test_get_unknown_static_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    if(get_static_method(env, factory_class, "org/msg/MessengerFactory","foobar","(Ljava/lang/String;)V") != NULL){
	fail("test_get_unknown_static_method");
    }
    succeed("test_get_unknown_static_method");
}

int test_get_known_static_method(){
//    get_static_method(JNIEnv* env, jclass class_object,char * class_name ,char * static_method_name, char * static_method_signature )

    running("test_get_known_static_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    if(get_static_method(env, factory_class, "org/msg/MessengerFactory","getMessenger","(Ljava/lang/String;)Lorg/msg/Messenger;") == NULL){
        fail("test_get_known_static_method");
    }
    succeed("test_get_known_static_method");
}

int test_call_static_method(){

    running("test_call_static_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","getMessenger","(Ljava/lang/String;)Lorg/msg/Messenger;") ;
    if( call_messenger_factory_create_method(env , "getMessenger", factory_class, method, "some_config_string") == NULL){
        fail("test_call_static_method");//factory method shud create jobject
    }  
    
    succeed("test_call_static_method");
}

int test_invoke_object_method(){

    running("test_invoke_object_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","getMessenger","(Ljava/lang/String;)Lorg/msg/Messenger;") ;
    jobject object = call_messenger_factory_create_method(env , "getMessenger", factory_class, method, "some_config_string");
   
    jclass interface_class = load_class(env, "org/msg/Messenger");
    jmethodID objectMethod = get_object_method( env, interface_class, "Messenger" ,"dispatch", "(Ljava/lang/String;)V" );

    if( invoke_msg_dispatch(env, object, objectMethod, "msg_body",  "topic" ) != 0){
      fail("test_invoke_object_method");
    } 
    succeed("test_invoke_object_method");
}

int test_invoke_object_method_failure(){

    running("test_invoke_object_method_failure");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","getMessenger","(Ljava/lang/String;)Lorg/msg/Messenger;") ;
    jobject object = call_messenger_factory_create_method(env , "getMessenger", factory_class, method, "some_config_string");
  
    jclass interface_class = load_class(env, "org/msg/Messenger");
    jmethodID objectMethod = get_object_method( env, interface_class, "Messenger" ,"dispatch", "(Ljava/lang/String;)V" );

    if( invoke_msg_dispatch(env, object, objectMethod, NULL,  "topic" ) == 0){ //PASS NULL SO THAT METHOD INVOKE FAILS ! npe excepted.
      fail("test_invoke_object_method_failure");
    }
    succeed("test_invoke_object_method_failure");
}

int test_get_known_object_method(){

    running("test_get_known_object_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","getMessenger","(Ljava/lang/String;)Lorg/msg/Messenger;") ;
    jobject object = call_messenger_factory_create_method(env , "getMessenger", factory_class, method, "some_config_string");
    
    jclass interface_class = load_class(env, "org/msg/Messenger");  
    jmethodID objectMethod = get_object_method( env, interface_class, "Messenger" ,"dispatch", "(Ljava/lang/String;)V" );
    if(objectMethod == NULL){
	 fail("test_get_known_object_method");
    }
    succeed("test_get_known_object_method");
}

int test_get_unknown_object_method(){

    running("test_get_unknown_object_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","getMessenger","(Ljava/lang/String;)Lorg/msg/Messenger;") ;
    jobject object = call_messenger_factory_create_method(env , "getMessenger", factory_class, method, "some_config_string");
    
    jclass interface_class = load_class(env, "org/msg/Messenger");     
    jmethodID objectMethod = get_object_method( env, interface_class, "Messenger" ,"fooobar", "(Ljava/lang/String;)V" );
    if(objectMethod != NULL){
         fail("test_get_unknown_object_method");
    }
    succeed("test_get_unknown_object_method");
}


/* This test had some side effects. some how after calling this test, 
when test `test_create_vm_with_good_arg` the jvm failed to find classes even though classpath was proper. todo: investigate
int test_create_vm_with_bad_arg(){
 
     running("test_create_vm_with_bad_arg");
     char * arg = "foobar";

     JNIEnv* env = create_vm(arg);
     if(env != NULL) {
        fail("test_create_vm_with_bad_arg");
	return FAILURE;
     } 
     return SUCCESS;
}
*/

int test_create_vm_with_good_arg(){

     running("test_create_vm_with_good_arg");
     char * arg = "-Djava.class.path=./classes";

     env = create_vm(arg);//static declaration above for env!

     if(env == NULL) {
        fail("test_create_vm_with_good_arg");
     }
     succeed("test_create_vm_with_good_arg");
}


int test_invoke_factory_destroy_method_failure(){

    running("test_invoke_factory_destroy_method_failure");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","destroy","(Ljava/lang/String;)V") ;


    if( call_messenger_factory_destroy_method(env, "destroy",factory_class ,method, NULL) == 0){ //PASS NULL SO THAT METHOD INVOKE FAILS ! npe excepted.
      fail("test_invoke_destroy_object_method_failure");
    }
    succeed("test_invoke_factory_destroy_method_failure");
}

int test_invoke_factory_destroy_method(){

    running("test_invoke_factory_destroy_method");
    jclass factory_class = load_class(env, "org/msg/MessengerFactory");
    jmethodID method = get_static_method(env, factory_class, "org/msg/MessengerFactory","destroy","(Ljava/lang/String;)V") ;

    if( call_messenger_factory_destroy_method(env, "destroy",factory_class ,method, "some_config") != 0){ 
      fail("test_invoke_destroy_object_method");
    }
    succeed("test_invoke_factory_destroy_method");
}

int main(int argc, char **argv) {

   //call this test as this creates the vm ............
   test_create_vm_with_good_arg() ; 

   ////////////////////////////////////
   if(env == NULL) { printf("Run test 'test_create_vm_with_good_arg' before running others. suite will exit"); return -1;} 
   ///////////////////////////////////

   test_load_unknown_class();
   test_load_known_class();

   test_get_unknown_static_method();
   test_get_known_static_method();

   test_call_static_method();
 test_get_unknown_object_method();
  test_get_known_object_method(); 

  test_invoke_object_method_failure();
  test_invoke_object_method();


  test_invoke_factory_destroy_method();
  test_invoke_factory_destroy_method_failure();


  printf("\n===========================================");
  printf("\n The tests have completed with %d failures!",failures);
  
  printf("\n===========================================\n");
}
