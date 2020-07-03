idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "")

spiffs_create_partition_image(spiffs web_root FLASH_IN_PROJECT)