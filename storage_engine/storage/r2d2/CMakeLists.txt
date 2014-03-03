# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#################################################################################
#### note: build requires jdk. we depend on jni.h header file and other jvm libs.
#http://lists.apple.com/archives/darwin-dev/2007/Jul/msg00080.html

# property `jni_h` = location of where jni.h header is available
# property `jvm_libs` = location of where libserver.dylib i.e. libjvm.dylib is available
#################################################################################

IF(UNIX AND NOT APPLE)
  SET(jni_h "/usr/lib/jvm/java-6-sun/include")
  SET(jvm_libs "/usr/lib/jvm/ia32-java-6-sun/jre/lib/i386/server")
ELSE(APPLE)
  SET (jvm_libs "/System/Library/Frameworks/JavaVM.framework")
  SET(jni_h "/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers")
ENDIF()


include_directories(${jni_h})

SET(R2D2_SOURCES ha_r2d2.cc ha_r2d2.h ha_jni.h ha_jni.c)

MYSQL_ADD_PLUGIN(r2d2 ${R2D2_SOURCES} STORAGE_ENGINE MODULE_ONLY)

TARGET_LINK_LIBRARIES(r2d2 ${jvm_libs})
