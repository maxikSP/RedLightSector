cmake_minimum_required(VERSION 2.8.4)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(PROJECT_NAME RedLightSector)
project(${PROJECT_NAME})

set(${CMAKE_PROJECT_NAME}_SKETCH RedLightSector.ino)

# Device name and port
set(${CMAKE_PROJECT_NAME}_BOARD uno)
set(${CMAKE_PROJECT_NAME}_PORT /dev/ttyACM0)
#set(${CMAKE_PROJECT_NAME}_PORT /dev/ttyUSB1)

generate_arduino_firmware(${CMAKE_PROJECT_NAME})

# Include custom libraries recursively
link_directories(/home/maxik/projects/arduino/testing/libraries/)
set(SimpleTimer_RECURSE True)
