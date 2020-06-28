idf_component_register(
                    INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} INTERFACE -DJSON_NOEXCEPTION)