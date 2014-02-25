#include <jni.h>

struct JVM{
     JNIEnv* env;
     JavaVM* jvm;
};

typedef struct JVM JVM;

#define SUCCESS_METHOD_INVOCATION 0;

#define FAILURE_METHOD_INVOCATION -1;

