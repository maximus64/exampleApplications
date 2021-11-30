project(Genetic)

find_package(matrixapplication REQUIRED)

add_executable(Genetic main.cpp genetic.cpp)
target_link_libraries(Genetic matrixapplication::matrixapplication matrixapplication::common)

install(TARGETS Genetic DESTINATION /opt/ledcube/apps)
