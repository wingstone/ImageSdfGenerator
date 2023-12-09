cmake_minimum_required(VERSION 3.15)

#------------------------
# project setting
#------------------------

project(ImageSdfGenertor)
set(TARGET ImageSdfGenertor)

#------------------------
# file setting
#------------------------

set(IMGUI_FILE )

file(GLOB INCLUDE_FILES src/*.h)
file(GLOB SOURCE_FILES src/*.cpp)
file(GLOB IMGUI_FILES ext/imgui/*.h ext/imgui/*.cpp)

#------------------------
# building
#------------------------

add_executable(${TARGET} ${INCLUDE_FILES} ${SOURCE_FILES} ${IMGUI_FILES})
target_include_directories(${TARGET} PUBLIC ext/imgui)
target_include_directories(${TARGET} PUBLIC ${INCLUDE_DIR})