/* Copyright (c) todo */

#include <stdio.h>
#include "myjni.h"

//bug http://stackoverflow.com/questions/6149930/create-jvm-after-destroying-it

//http://stackoverflow.com/questions/2093112/why-i-should-not-reuse-a-jclass-and-or-jmethodid-in-jni

//javap -classpath . r2d2.msg.Messenger -s

static  JNIEnv* jni_env_ref = NULL; //we keep one refernce to jvm env object. please make sure that u create only one.

void handleError(JNIEnv *env)
{
    /*  A simple routine to report and handle an exception.  */
    (*env)->ExceptionDescribe(env); /* write exception data to the console */
    (*env)->ExceptionClear(env);    /* clear the exception that was pending */
}


/**
JNI method wrapper
Returns a class object from a fully-qualified name, or NULL if the class cannot be found.
*/
jclass load_class(JNIEnv* env, char * class_name ){
   jclass class_object = (*env)->FindClass(env, class_name);
   if ( (*env)->ExceptionOccurred(env)) {
        fprintf(stderr,"Error occured while loading class: %s\n",class_name);
        handleError(env);
        return NULL;
   }
   return class_object;
}

jmethodID get_static_method(JNIEnv* env, jclass class_object,char * class_name ,char * static_method_name, char * static_method_signature ){
   jmethodID staticMethod = (*env)->GetStaticMethodID(env, class_object, static_method_name , static_method_signature);
   if ((*env)->ExceptionOccurred(env)){
           fprintf(stderr,"Error occured while getting static method: %s in class: %s\n",static_method_name,class_name);
           handleError(env);
           return NULL;
   }
   return staticMethod;
}

jmethodID get_object_method(JNIEnv* env, jclass class_object, char * class_name ,char * method_name, char * method_signature ){
   jmethodID objectMethod = (*env)->GetMethodID(env, class_object, method_name , method_signature);
   if ((*env)->ExceptionOccurred(env)){
              fprintf(stderr,"Error occured while getting non static method: %s in class: %s\n",method_name,class_name);
              handleError(env);
              return NULL;
   }
   return objectMethod;
}

jobject call_messenger_factory_method(JNIEnv* env , char * static_method_name, jclass class_object, jmethodID staticMethod , char * config){
    jstring config_string = (*env)->NewStringUTF(env,config);

    jobject object = (*env)->CallStaticObjectMethod(env, class_object, staticMethod, config_string);
    if ((*env)->ExceptionOccurred(env)){
                  fprintf(stderr,"Error occured while calling static factory method: %s\n",static_method_name);
                  handleError(env);
                  return NULL;
    }
    (*env)->DeleteLocalRef(env, config_string );
    return object;
}

int invoke_msg_dispatch(JNIEnv* env, jobject java_object, jmethodID objectMethod, char * msg_body, char * topic ){

    jstring body_string = (*env)->NewStringUTF(env,msg_body);

    printf("\nInvoking on object calling java method from c. !\n");

    (*env)->CallVoidMethod(env, java_object, objectMethod, body_string );

    if ((*env)->ExceptionOccurred(env)){
                  fprintf(stderr,"Error occured while msg dispatch to topic : %s \n",topic);
                  handleError(env);
                  return FAILURE_METHOD_INVOCATION;
    }
    (*env)->DeleteLocalRef(env, body_string );

    return SUCCESS_METHOD_INVOCATION;
}

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
        // todo: cache the instances of objects created in jvm else hash code of object will change
}

/**
create jvm using jni. Note: method is not synchronized.
*/
JNIEnv* create_vm(char * class_path) {

        printf("Creating jvm with class path %s\n",class_path);

        if(jni_env_ref != NULL) //check if allready created then return existing instance.
        {
             printf("Returning previously Created jvm with class path %s\n",class_path);
             return jni_env_ref;
        }

        JavaVM* jvm;
        JNIEnv* env;
        JavaVMInitArgs args;
        JavaVMOption options[2];

        options[0].optionString = class_path;
        options[1].optionString = "-verbose:jni";

        args.version = JNI_VERSION_1_6;
        args.nOptions = 2;
        args.options = options;
        args.ignoreUnrecognized = JNI_FALSE;

        jint jvm_creation_err_Code =JNI_CreateJavaVM(&jvm, (void **)&env, &args);

        if(jvm_creation_err_Code < 0)
        {
            fprintf(stderr, "JVM creation failed with error code: %d \n",jvm_creation_err_Code);
         	return NULL;
        }

        printf("Created jvm with class path %s\n",class_path);

        jni_env_ref = env; //set the static instance.

        return env;
}