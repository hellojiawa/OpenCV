# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# opencv
include_directories(C:/Users/admin/Desktop/opencv3-android-sdk-with-contrib-master/OpenCV-android-sdk/sdk/native/jni/include)
add_library(lib_opencv SHARED IMPORTED)
set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libopencv_java3.so)

aux_source_directory(src/main/cpp/ DIR_LIB_SRCS)
file(GLOB native_srcs "src/main/cpp/*.cpp")
add_library(
        funsnap_target

        SHARED

        src/main/cpp/AlgConnected.c
        src/main/cpp/AlgContourExtract.cpp
        src/main/cpp/AlgCvt.cpp
        src/main/cpp/AlgKmeans.cpp
        src/main/cpp/AlgObjStructDef.c
        src/main/cpp/AlgTk_baseStruct.c
        src/main/cpp/AlgLib.cpp)


find_library(
        log-lib

        log)


target_link_libraries(
        funsnap_target

        jnigraphics

        ${log-lib})




add_library(
        mat_util

        SHARED

        src/main/cpp/hand-lib.cpp)


target_link_libraries(
        mat_util
        lib_opencv

        ${log-lib})