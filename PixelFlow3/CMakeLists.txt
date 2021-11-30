project(PixelFlow3)

find_package(matrixapplication REQUIRED)

set(MAINSRC
    main.cpp pixelflow.cpp pixelflow.h)

set(MAINLIBS
        matrixapplication::matrixapplication
        matrixapplication::common
)

add_executable(PixelFlow3 ${MAINSRC})
target_link_libraries(PixelFlow3 ${MAINLIBS}) #-l flag

install(TARGETS PixelFlow3 DESTINATION /opt/ledcube/apps)
