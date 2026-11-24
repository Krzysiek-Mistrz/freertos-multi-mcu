# Application sources live in projects/common, embedkit is a git submodule at the repo root.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

idf_component_register(
    SRCS "main.c" "bsp_esp32.c"
         "${REPO_ROOT}/projects/common/app_pipeline.c"
         "${REPO_ROOT}/projects/common/app_primitives.c"
         "${REPO_ROOT}/embedkit/lib/fsm.c"
    INCLUDE_DIRS "." "${REPO_ROOT}/projects/common" "${REPO_ROOT}/embedkit/lib"
    REQUIRES driver esp_hw_support
)
