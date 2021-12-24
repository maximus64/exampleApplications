project(Glow)

find_package(matrixapplication REQUIRED)

add_executable(Glow main.cpp Glow.cpp)
target_link_libraries(Glow
    matrixapplication::matrixapplication
    matrixapplication::common)

install(TARGETS Glow DESTINATION /opt/ledcube/apps)
