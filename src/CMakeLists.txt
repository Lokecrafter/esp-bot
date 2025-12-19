# Hittar alla .cpp och .c filer i src-mappen automatiskt
file(GLOB_RECURSE app_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.*")

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_adc
)