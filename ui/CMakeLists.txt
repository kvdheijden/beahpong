find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)

add_executable(ui main.cpp)
target_include_directories(ui PRIVATE ${OPENGL_INCLUDE_DIRS} ${X11_INCLUDE_DIRS})
target_link_libraries(ui ${OPENGL_LIBRARIES} ${X11_LIBRARIES})