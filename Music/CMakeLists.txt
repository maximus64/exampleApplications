project(Music)

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(matrixapplication REQUIRED)
find_package(FFmpeg REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIR})

add_executable(Music main.cpp music.cpp AudioDecoder.cpp)
target_link_libraries(Music
    matrixapplication::matrixapplication
    matrixapplication::common
    ${FFMPEG_LIBRARIES}
    swresample
    libasound.so
    fftw3)

install(TARGETS Music DESTINATION /opt/ledcube/apps)
