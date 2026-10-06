cmake_minimum_required(VERSION 3.16)
project(tdt_interface)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/SendData.msg"
  "msg/AutoAimState.msg"
  "msg/AutoAimFrame.msg"
  "msg/RuneRequest.msg"
  "msg/RuneResult.msg"
  DEPENDENCIES builtin_interfaces std_msgs sensor_msgs geometry_msgs
)

install(DIRECTORY
  msg
  DESTINATION share/${PROJECT_NAME}/
)

ament_package()
