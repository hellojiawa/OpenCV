# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# opencv
include_directories(C:/Users/admin/Desktop/opencv3-android-sdk-with-contrib-master/OpenCV-android-sdk/sdk/native/jni/include)
add_library( lib_opencv SHARED IMPORTED )
set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libopencv_java3.so)


aux_source_directory(src/main/cpp/ DIR_LIB_SRCS)
file(GLOB native_srcs "src/main/cpp/*.cpp")
add_library( # Sets the name of the library.
             mat_util

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             # ${native_srcs}
              src/main/cpp/hand-lib.cpp)


find_library( # Sets the name of the path variable.
              mat_util

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )


target_link_libraries( # Specifies the target library.
                       mat_util

                       lib_opencv

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )