#include <jni.h>

struct JVM{
     JNIEnv* env;
     JavaVM* jvm;
};

typedef struct JVM JVM;

