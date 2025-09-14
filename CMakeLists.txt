cmake_minimum_required(VERSION 3.28)
project(sigcraft)

set(CMAKE_CXX_STANDARD 20)

set(IMR_FOLDER "" CACHE STRING "Folder where IMR is found")
add_subdirectory(${IMR_FOLDER} imr_build)

if (NOT TARGET imr)
    message("IMR not found. Please set IMR_FOLDER accordingly.")
endif ()

add_subdirectory(enklume)

find_package(nasl)

add_executable(sigcraft main.cpp camera.cpp chunk_mesh.cpp world.cpp)
target_link_libraries(sigcraft imr enklume nasl::nasl)


add_custom_target(raygen_spv COMMAND ${GLSLANG_EXE} -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/raygen.rgen -o ${CMAKE_CURRENT_BINARY_DIR}/raygen.rgen.spv --target-env vulkan1.3)
add_dependencies(sigcraft raygen_spv)
add_custom_target(closesthit_spv COMMAND ${GLSLANG_EXE} -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/closesthit.rchit -o ${CMAKE_CURRENT_BINARY_DIR}/closesthit.rchit.spv --target-env vulkan1.3)
add_dependencies(sigcraft closesthit_spv)
add_custom_target(miss_spv COMMAND ${GLSLANG_EXE} -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/miss.rmiss -o ${CMAKE_CURRENT_BINARY_DIR}/miss.rmiss.spv --target-env vulkan1.3)
add_dependencies(sigcraft miss_spv)
