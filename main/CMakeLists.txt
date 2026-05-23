idf_component_register(
    SRCS "main.c"
         "acc_algorithm.c"
         "acc_control_helper.c"
         "acc_hal_integration_espidf_xe121.c"
         "acc_integration_esp32.c"
         "acc_integration_log.c"
         "acc_processing_helpers.c"
    INCLUDE_DIRS "../include"
)

add_prebuilt_library(libacc_detector_distance_a121 "${CMAKE_CURRENT_SOURCE_DIR}/../lib/libacc_detector_distance_a121.a")
add_prebuilt_library(libacc_detector_presence_a121 "${CMAKE_CURRENT_SOURCE_DIR}/../lib/libacc_detector_presence_a121.a")
add_prebuilt_library(libacconeer_a121 "${CMAKE_CURRENT_SOURCE_DIR}/../lib/libacconeer_a121.a")

target_link_libraries(${COMPONENT_TARGET} INTERFACE libacc_detector_presence_a121 libacc_detector_distance_a121 libacconeer_a121)

include(CheckCCompilerFlag)
check_c_compiler_flag("-ffile-prefix-map=foo=bar" SUPPORTS_FFILE_PREFIX_MAP)
if(SUPPORTS_FFILE_PREFIX_MAP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffile-prefix-map=${CMAKE_CURRENT_LIST_DIR}=.")
endif()
