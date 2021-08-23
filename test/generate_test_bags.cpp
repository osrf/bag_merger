// Copyright 2021, Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <example_interfaces/msg/int32.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialization.hpp>
#include <rcutils/filesystem.h>
#include <rosbag2_cpp/writers/sequential_writer.hpp>
#include <rosbag2_storage/serialized_bag_message.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>


void prepare_input_bag(
  int num_topics = 1,
  int num_samples = 5,
  int start_data = 0,
  int start_time_offset = 0,
  int time_increment = 100,
  std::string bag_path = "input_bag_0")
{
  rcutils_mkdir(bag_path.c_str());

  const rosbag2_cpp::StorageOptions storage_options({bag_path, "sqlite3"});
  const rosbag2_cpp::ConverterOptions converter_options(
    {rmw_get_serialization_format(), rmw_get_serialization_format()});
  std::unique_ptr<rosbag2_cpp::writers::SequentialWriter> writer =
    std::make_unique<rosbag2_cpp::writers::SequentialWriter>();
  writer->open(storage_options, converter_options);

  for (int ii = 0; ii < num_topics; ++ii) {
    std::stringstream topic_name;
    topic_name << bag_path << "_topic_" << ii;
    writer->create_topic(
      {topic_name.str(),
        "example_interfaces/msg/Int32",
        rmw_get_serialization_format(),
        ""});
  }

  int topic_number = 0;
  int timestamp = start_time_offset;
  auto serializer = rclcpp::Serialization<example_interfaces::msg::Int32>();
  for (int data_value = start_data; data_value < start_data + num_samples; ++data_value) {
    auto serialized_message = rclcpp::SerializedMessage();
    example_interfaces::msg::Int32 data;
    data.data = data_value;
    serializer.serialize_message(&data, &serialized_message);

    std::stringstream topic_name;
    topic_name << bag_path << "_topic_" << topic_number;

    auto bag_message = std::make_shared<rosbag2_storage::SerializedBagMessage>();
    bag_message->serialized_data = std::shared_ptr<rcutils_uint8_array_t>(
      new rcutils_uint8_array_t,
      [](rcutils_uint8_array_t * msg) {
        auto fini_return = rcutils_uint8_array_fini(msg);
        delete msg;
        if (fini_return != RCUTILS_RET_OK) {
          std::cerr << "Failed to destroy serialized message " <<
          rcutils_get_error_string().str;
        }
      });
    *bag_message->serialized_data = serialized_message.release_rcl_serialized_message();

    bag_message->topic_name = topic_name.str();
    bag_message->time_stamp = timestamp;

    writer->write(bag_message);

    timestamp += time_increment;
    topic_number = (topic_number + 1) % num_topics;
  }
}


int main(int argc, char ** argv)
{
  if (argc < 6) {
    std::cerr << "Usage: " << argv[0] <<
      " [num_topics] [num_samples] [start_data] [start_time_offset] [time_increment] [bag_path]\n";
    return 1;
  }

  int num_topics = std::atoi(argv[1]);
  int num_samples = std::atoi(argv[2]);
  int start_data = std::atoi(argv[3]);
  int start_time_offset = std::atoi(argv[4]);
  int time_increment = std::atoi(argv[5]);
  char * bag_path = argv[6];

  prepare_input_bag(
    num_topics,
    num_samples,
    start_data,
    start_time_offset,
    time_increment,
    bag_path);

  return 0;
}
