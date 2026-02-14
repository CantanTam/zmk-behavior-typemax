# 这里的 app 是 ZMK 的主目标
target_include_directories(app PRIVATE include)

# 指向你实际的源文件路径（相对于 CMakeLists.txt 的路径）
target_sources(app PRIVATE src/behaviors/behavior_led.c)
target_sources(app PRIVATE src/behaviors/behavior_blink.c)
target_sources(app PRIVATE src/behaviors/behavior_battery.c)
target_sources(app PRIVATE src/behaviors/behavior_reclayer.c)
target_sources(app PRIVATE src/behaviors/behavior_tolayer.c)
target_sources(app PRIVATE src/behaviors/behavior_togtouch.c)
target_sources(app PRIVATE src/behaviors/behavior_touch_init.c)


target_sources(app PRIVATE src/behaviors/behavior_xw12a_power.c)
target_sources(app PRIVATE src/behaviors/behavior_ws2812.c)
target_sources(app PRIVATE drivers/sensor/xw12a/xw12a.c)
#target_sources(app PRIVATE drivers/sensor/xw12a/ws2812.c)