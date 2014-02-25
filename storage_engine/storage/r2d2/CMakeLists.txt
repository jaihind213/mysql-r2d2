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

#SET(R2D2_SOURCES  ha_r2d2.cc ha_r2d2.h)
#MYSQL_ADD_PLUGIN(r2d2 ${R2D2_SOURCES} STORAGE_ENGINE)

SET (jvm_libs "/System/Library/Frameworks/JavaVM.framework")
###LINK_DIRECTORIES(${jvm_libs})

SET(jni_h "/Developer/SDKs/MacOSX10.6.sdk/System/Library/Frameworks/JavaVM.framework/Versions/1.6.0/Headers")
include_directories(${jni_h})

SET(R2D2_SOURCES ha_r2d2.cc ha_r2d2.h ha_jni.h ha_jni.c)

MYSQL_ADD_PLUGIN(r2d2 ${R2D2_SOURCES} STORAGE_ENGINE)

TARGET_LINK_LIBRARIES(r2d2 ${jvm_libs})
