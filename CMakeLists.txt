zephyr_include_directories(include)
zephyr_library()

zephyr_library_sources_ifdef(
        CONFIG_DONGLE_SCREEN_KEYBOARD_STATUS
        boards/shields/dongle_screen/src/widgets/keyboard_state.c
)

if(CONFIG_SHIELD_DONGLE_SCREEN)

        add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR}/drivers/display)

endif()
