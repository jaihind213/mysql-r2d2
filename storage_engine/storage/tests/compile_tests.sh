# tests compiled for mac os - have to change this.

cc -framework JavaVM -I/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers -c ../r2d2/ha_jni.c ../r2d2/ha_jni.h

cc -framework JavaVM -I/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers test_jni.c -o run_tests.o ha_jni.o


echo "____"
echo "if compilation succeeds run tests as follows './run_tests.o'"
echo "____"
