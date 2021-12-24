project(HBD)

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(matrixapplication REQUIRED)
find_package(FFmpeg REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIR})

add_executable(HBD main.cpp HBD.cpp AudioDecoder.cpp)
target_link_libraries(HBD
    matrixapplication::matrixapplication
    matrixapplication::common
    ${FFMPEG_LIBRARIES}
    swresample
    libasound.so)

install(TARGETS HBD DESTINATION /opt/ledcube/apps)
