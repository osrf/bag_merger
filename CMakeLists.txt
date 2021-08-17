cmake_minimum_required(VERSION 3.5)
project(bag_merger)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rcutils REQUIRED)
find_package(rosbag2_cpp REQUIRED)
find_package(rosbag2_storage REQUIRED)
find_package(example_interfaces REQUIRED)

add_executable(merge_bags src/merge_bags.cpp)
ament_target_dependencies(merge_bags
  rclcpp
  rcutils
  rosbag2_cpp
)

install(TARGETS merge_bags
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  ament_lint_auto_find_test_dependencies()

  add_executable(generate_test_bags
    test/generate_test_bags.cpp)
  if(TARGET generate_test_bags)
    ament_target_dependencies(generate_test_bags
      rclcpp
      rcutils
      rosbag2_cpp
      rosbag2_storage
      example_interfaces
    )
    install(TARGETS generate_test_bags
      DESTINATION lib/${PROJECT_NAME}
    )
  endif()

  add_executable(print_bag
    test/print_bag.cpp)
  if(TARGET print_bag)
    ament_target_dependencies(print_bag
      rclcpp
      rosbag2_cpp
      rosbag2_storage
      example_interfaces
    )
    install(TARGETS print_bag
      DESTINATION lib/${PROJECT_NAME}
    )
  endif()
endif()

ament_package()
