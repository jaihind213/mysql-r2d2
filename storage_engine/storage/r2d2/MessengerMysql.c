/* Copyright (c) todo */

#include <stdio.h>
#include "myjni.h"

//bug http://stackoverflow.com/questions/6149930/create-jvm-after-destroying-it

static  JNIEnv* jni_env_ref = NULL;
void invoke_on_object(JNIEnv* env, char * class_name, char * interface_class_name ,char * staticMethodSign ,char * static_factory_method_name, char * object_method , char * objectMethodSignature, char * config, char * msg_body, char * topic){

        jclass helloWorldClass = (*env)->FindClass(env, class_name);
        jclass InterfaceClass = (*env)->FindClass(env, interface_class_name);
        jmethodID staticMethod = (*env)->GetStaticMethodID(env, helloWorldClass, static_factory_method_name , staticMethodSign);

        jmethodID objectMethod = (*env)->GetMethodID(env, InterfaceClass, object_method , objectMethodSignature);


        jstring config_string = (*env)->NewStringUTF(env,config); 
        jobject hello_object = (*env)->CallStaticObjectMethod(env, helloWorldClass, staticMethod, config_string);


         jstring body_string = (*env)->NewStringUTF(env,msg_body);
 
         jstring topic_string = (*env)->NewStringUTF(env,topic); 

        printf("\nInvoking on object calling java method from c again !\n");
        (*env)->CallVoidMethod(env, hello_object, objectMethod, body_string, topic_string );

//	env->DeleteLocalRef(config_string); //TODO 
//     todo: cache the instances of objects created in jvm else hash code of object will change
}


JNIEnv* create_vm(char * class_path) {

        printf("Creating jvm with class path %s\n\n",class_path);
        if(jni_env_ref != NULL){
             return jni_env_ref;
        }

        JavaVM* jvm;
        JNIEnv* env;
        JavaVMInitArgs args;
        JavaVMOption options[1];

        args.version = JNI_VERSION_1_6;
        
        args.nOptions = 1;
        options[0].optionString = class_path;
        args.options = options;
        args.ignoreUnrecognized = JNI_FALSE;

        jint jvm_creation_err_Code =JNI_CreateJavaVM(&jvm, (void **)&env, &args);
        if(jvm_creation_err_Code < 0){
                printf("@@#");
                fprintf(stderr, "JVM creation failed with error code: %d \n",jvm_creation_err_Code);
         	return NULL;
        }
         //JVM_SHARE.jvm = jvm;
         //JVM_SHARE.env = env;
         printf("Created jvm with class path %s\n\n",class_path);
        jni_env_ref = env;
        return env;               //JNI_DestroyJavaVM()
}
/*
int main(int argc, char **argv) {

        //(Ljava/lang/String;)Lr2d2/msg/Messenger;

        JNIEnv* env = create_vm("-Djava.class.path=/Users/vishnuhr/volume/mysql-r2d2/r2d2-java/r2d2-java/target/classes");
        char * className = "r2d2/msg/MessengerFactory";
        char * staticMethodName = "getMessenger";
	char * interface_class_name = "r2d2/msg/Messenger";
        char * objectMethodName = "dispatch";

        char * staticMethodSignature = "(Ljava/lang/String;)Lr2d2/msg/Messenger;"; 
        char * objectMethodSignature = "(Ljava/lang/String;Ljava/lang/String;)V"; 

        invoke_on_object(env, className,interface_class_name ,staticMethodSignature ,staticMethodName, objectMethodName, objectMethodSignature, "type:BLACKHOLE;k:v","body","topic");
printf("\n********");
}
*/


