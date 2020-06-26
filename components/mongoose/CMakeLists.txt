idf_component_register(SRCS "mongoose/mongoose.c"
                    INCLUDE_DIRS "mongoose")

target_compile_options(${COMPONENT_LIB} PRIVATE -D MG_ENABLE_HTTP_STREAMING_MULTIPART=1)