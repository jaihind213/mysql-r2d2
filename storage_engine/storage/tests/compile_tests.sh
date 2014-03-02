#!/bin/sh

# tests compiled for mac os - have to change this.

if [ -f run_tests.o ] ;then
   rm -rf run_tests.o
fi
 
echo "Compiling java src..."
mkdir -p classes

javac -cp java/ java/org/msg/*.java -d classes/

if [[ $? != 0 ]] ; then
	echo "java src compile failed!" 
        exit -1
fi


echo "Compiling JNI src..."
cc -framework JavaVM -I/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers -c ../r2d2/ha_jni.c ../r2d2/ha_jni.h
if [[ $? != 0 ]] ; then
        echo "jni src compile failed!"
        exit -1
fi

echo "Compiling TEST src..."
cc -framework JavaVM -I/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers test_jni.c -o run_tests.o ha_jni.o
if [[ $? != 0 ]] ; then        
	echo "test src compile failed!"
        exit -1
fi


echo "____"
echo "Compilation was successful. run tests as follows './run_tests.o'"
echo "____"

