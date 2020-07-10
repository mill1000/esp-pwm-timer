# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-led-control)

# Extra target to generate OTA tarball
set(OTA_TARBALL "${PROJECT_NAME}-ota.tar")
add_custom_target(ota_tarball
    COMMAND tar -cf ${OTA_TARBALL} ${PROJECT_NAME}.bin spiffs.bin
    COMMAND ${CMAKE_COMMAND} -E echo "Generated ${build_dir}/${OTA_TARBALL}"
    DEPENDS gen_project_binary spiffs_spiffs_bin
    BYPRODUCTS ${OTA_TARBALL}
    COMMENT "Generating OTA tarball from app and SPIFFS image"
    VERBATIM
    )

# Add the OTA tarball as an additional app dependency
add_dependencies(app ota_tarball)