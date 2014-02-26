#include "../r2d2/ha_jni.h"

static JNIEnv* env ;

#define SUCCESS 0
#define FAILURE 1

#define running(test) printf("\n----> Running Test: %s ..\n",test);

void fail(char * test){
 printf("\n---------> !!!!! TEST_FAILURE: %s ..\n",test);
 exit(FAILURE);
}

int test_load_unknown_class(){

     running("test_load_unknown_class");
     if(load_class(env, "fooobar" ) != NULL ){
        fail("test_load_unknown_class");
        return FAILURE;
     }
     return SUCCESS;
}

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

int test_create_vm_with_good_arg(){

     running("test_create_vm_with_good_arg");
     char * arg = "-DTEST_VAR=1";

     env = create_vm(arg);//static declaration above for env!

     if(env == NULL) {
        fail("test_create_vm_with_good_arg");
        return FAILURE;
     }
     return SUCCESS;
}

int main(int argc, char **argv) {
   test_create_vm_with_bad_arg();

   //call this test as this creates the vm ............
   test_create_vm_with_good_arg() ; 

   ////////////////////////////////////
   if(env == NULL) { printf("Run test 'test_create_vm_with_good_arg' before running others. suite will exit"); return -1;} 
   ///////////////////////////////////

   test_load_unknown_class();
}
