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
#include <rosbag2_cpp/readers/sequential_reader.hpp>
#include <rosbag2_cpp/reader.hpp>
#include <rosbag2_storage/serialized_bag_message.hpp>

#include <iostream>
#include <memory>


void read_and_print_bag(std::string uri)
{
  std::unique_ptr<rosbag2_cpp::readers::SequentialReader> reader_impl =
    std::make_unique<rosbag2_cpp::readers::SequentialReader>();
  const rosbag2_cpp::StorageOptions storage_options({uri, "sqlite3"});
  const rosbag2_cpp::ConverterOptions converter_options(
    {rmw_get_serialization_format(),
      rmw_get_serialization_format()});
  reader_impl->open(storage_options, converter_options);
  rosbag2_cpp::Reader reader(std::move(reader_impl));

  auto serializer = rclcpp::Serialization<example_interfaces::msg::Int32>();
  while (reader.has_next()) {
    auto message = reader.read_next();
    example_interfaces::msg::Int32 data;


    //auto serialized_message = rclcpp::SerializedMessage(message->serialized_data);

    //serializer.deserialize_message(message->serialized_data, &data);

    std::cout << "Topic: " << message->topic_name << "\tData: " << data.data << "\tTime stamp: " <<
      message->time_stamp << '\n';
  }
}


int main(int argc, char ** argv)
{
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <bag file>\n";
    return 1;
  }
  read_and_print_bag(std::string(argv[1]));
  return 0;
}
