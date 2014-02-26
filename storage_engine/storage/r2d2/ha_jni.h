#include <jni.h>

#define SUCCESS_METHOD_INVOCATION 0;

#define FAILURE_METHOD_INVOCATION -1;

JNIEnv* create_vm(char * class_path);

jclass load_class(JNIEnv* env, char * class_name );

jmethodID get_static_method(JNIEnv* env, jclass class_object,char * class_name ,char * static_method_name, char * static_method_signature )   ;

jmethodID get_object_method(JNIEnv* env, jclass class_object, char * class_name ,char * method_name, char * method_signature );

jobject call_messenger_factory_create_method(JNIEnv* env, char * method_name, jclass class_object, jmethodID staticMethod , char * config);

int call_messenger_factory_destroy_method(JNIEnv* env, char * method_name, jclass class_object, jmethodID staticMethod , char * config);

int invoke_msg_dispatch(JNIEnv* env, jobject java_object, jmethodID objectMethod, char * msg_body, char * topic );
