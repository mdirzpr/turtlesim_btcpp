cmake_minimum_required(VERSION 3.22)
project(bt_turtlesim_ctrl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Dependencies
# ============================================================================
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(behaviortree_cpp REQUIRED)
find_package(turtlesim REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(bt_turtlesim_interfaces REQUIRED)

include_directories(
  include
  ${behaviortree_cpp_INCLUDE_DIRS}
)

# ============================================================================
# turtle_controller — BT executor that drives turtle1 to hunt prey
# ============================================================================
add_executable(turtle_controller
  src/turtle_controller_node.cpp
  src/turtle_bt_nodes.cpp
)
ament_target_dependencies(turtle_controller
  rclcpp
  ament_index_cpp
  behaviortree_cpp
  turtlesim
  geometry_msgs
  bt_turtlesim_interfaces
)

# ============================================================================
# turtle_spawner — periodically spawns prey turtles at random positions
# ============================================================================
add_executable(turtle_spawner
  src/turtle_spawner_node.cpp
)
ament_target_dependencies(turtle_spawner
  rclcpp
  turtlesim
  bt_turtlesim_interfaces
)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS
  turtle_controller
  turtle_spawner
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

install(DIRECTORY bt_structures/
  DESTINATION share/${PROJECT_NAME}/bt_structures
)

install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

ament_package()
