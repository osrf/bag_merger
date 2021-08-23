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

#include <rcutils/filesystem.h>

#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialization.hpp>
#include <rosbag2_cpp/reader.hpp>
#include <rosbag2_cpp/readers/sequential_reader.hpp>
#include <rosbag2_cpp/writer.hpp>
#include <rosbag2_cpp/writers/sequential_writer.hpp>

#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using ReaderPtr = std::shared_ptr<rosbag2_cpp::Reader>;
using NextMessage = std::optional<std::shared_ptr<rosbag2_storage::SerializedBagMessage>>;
struct ReaderWithNext
{
  ReaderPtr reader;
  NextMessage next_message;
};
using ReaderStore = std::vector<ReaderWithNext>;


std::pair<std::vector<std::string>, std::optional<std::string>> get_options(int argc, char ** argv)
{
  std::pair<std::vector<std::string>, std::optional<std::string>> empty_result{{}, std::nullopt};

  // There must be at least 5 arguments:
  // program name, -o, output destination, input bag 1, input bag 2
  if (argc < 5) {
    std::cerr << "Usage: " << argv[0] << " -o <output bag> <input bag> <input bag...>\n";
    return empty_result;
  }

  std::vector<std::string> inputs;
  std::optional<std::string> output = std::nullopt;

  std::string flag = "-o";
  for (int ii = 1; ii < argc; ++ii) {
    if (flag == argv[ii]) {
      if (ii + 1 == argc) {
        std::cerr << "Missing argument to output flag\n";
        return empty_result;
      }
      output = std::string(argv[ii + 1]);
      ii += 1;
    } else {
      inputs.push_back(argv[ii]);
    }
  }

  return std::make_pair(inputs, output);
}


ReaderStore make_readers(const std::vector<std::string> & input_names)
{
  ReaderStore result;

  for (const auto & input_name : input_names) {
    std::unique_ptr<rosbag2_cpp::readers::SequentialReader> reader_impl =
      std::make_unique<rosbag2_cpp::readers::SequentialReader>();
    const rosbag2_cpp::StorageOptions storage_options({input_name, "sqlite3"});
    const rosbag2_cpp::ConverterOptions converter_options(
      {rmw_get_serialization_format(),
        rmw_get_serialization_format()});
    reader_impl->open(storage_options, converter_options);

    std::shared_ptr<rosbag2_cpp::Reader> reader =
      std::make_shared<rosbag2_cpp::Reader>(std::move(reader_impl));
    if (reader->has_next()) {
      result.push_back({reader, reader->read_next()});
    } else {
      result.push_back({reader, std::nullopt});
    }
  }

  return result;
}


std::unique_ptr<rosbag2_cpp::Writer> make_writer(const std::string & output_name)
{
  const rosbag2_cpp::StorageOptions storage_options({output_name, "sqlite3"});
  const rosbag2_cpp::ConverterOptions converter_options(
    {rmw_get_serialization_format(),
      rmw_get_serialization_format()});
  std::unique_ptr<rosbag2_cpp::writers::SequentialWriter> writer_impl =
    std::make_unique<rosbag2_cpp::writers::SequentialWriter>();
  writer_impl->open(storage_options, converter_options);

  return std::make_unique<rosbag2_cpp::Writer>(std::move(writer_impl));
}


std::vector<rosbag2_storage::TopicMetadata> combine_input_topics(const ReaderStore & readers)
{
  std::vector<rosbag2_storage::TopicMetadata> result;

  for (const auto & r : readers) {
    auto topic_metadata = r.reader->get_all_topics_and_types();
    for (auto && t : topic_metadata) {
      auto existing_topic = std::find_if(
        result.begin(),
        result.end(),
        [&t](auto topic) {
          return topic.name == t.name;
        });
      if (existing_topic == result.end()) {
        result.emplace_back(std::move(t));
      }
      // Ignore already-listed topics
    }
  }
  return result;
}


void set_output_metadata(
  std::unique_ptr<rosbag2_cpp::Writer> & writer,
  const std::vector<rosbag2_storage::TopicMetadata> & topics)
{
  for (const auto & t : topics) {
    writer->create_topic(t);
  }
}


uint64_t get_total_message_count(const ReaderStore & readers)
{
  uint64_t total = 0;
  for (const auto & r : readers) {
    total += r.reader->get_metadata().message_count;
  }
  return total;
}


std::optional<ReaderStore::iterator> get_earliest_reader(ReaderStore & readers)
{
  rcutils_time_point_value_t earliest_time = std::numeric_limits<rcutils_time_point_value_t>::max();
  ReaderStore::iterator earliest_message = readers.end();

  for (ReaderStore::iterator current = readers.begin(); current != readers.end(); ++current) {
    if (current->next_message != std::nullopt) {
      if (current->next_message.value()->time_stamp < earliest_time) {
        earliest_time = current->next_message.value()->time_stamp;
        earliest_message = current;
      }
    }
  }

  if (earliest_message == readers.end()) {
    return std::nullopt;
  } else {
    return earliest_message;
  }
}


std::optional<std::shared_ptr<rosbag2_storage::SerializedBagMessage>> read_next(
  ReaderStore & readers)
{
  auto reader_with_next = get_earliest_reader(readers);
  if (reader_with_next == std::nullopt) {
    return std::nullopt;
  }

  auto result = reader_with_next.value()->next_message.value();
  if (reader_with_next.value()->reader->has_next()) {
    reader_with_next.value()->next_message = reader_with_next.value()->reader->read_next();
  } else {
    reader_with_next.value()->next_message = std::nullopt;
  }
  return result;
}


void write_next_message(
  std::unique_ptr<rosbag2_cpp::Writer> & writer,
  const std::shared_ptr<rosbag2_storage::SerializedBagMessage> & message)
{
  writer->write(message);
}


int main(int argc, char ** argv)
{
  auto inputs_and_output = get_options(argc, argv);
  auto inputs = inputs_and_output.first;
  auto output = inputs_and_output.second;

  if (inputs.size() == 0) {
    std::cerr << "Missing input bags\n";
    return 1;
  }
  if (output == std::nullopt) {
    std::cerr << "Missing output bag name\n";
    return 1;
  }

  // Create a reader for each input bag
  ReaderStore readers = make_readers(inputs);
  if (readers.size() == 0) {
    return 1;
  }
  // Create the output directory
  if (rcutils_exists(output.value().c_str())) {
    std::cerr << "Output bag directory already exists\n";
    return 1;
  }
  rcutils_mkdir(output.value().c_str());
  // Create a writer for the output bag
  std::unique_ptr<rosbag2_cpp::Writer> writer = make_writer(output.value());

  // Combine the input bag topics into one list and use it for the output bag metadata
  auto input_topics = combine_input_topics(readers);
  set_output_metadata(writer, input_topics);

  auto num_messages = get_total_message_count(readers);
  std::cout << "Processing " << num_messages << " messages from " << readers.size() <<
    " input bags\n";
  std::cout << std::unitbuf;
  std::cout << "  0%";
  uint64_t processed_count = 0;
  // Loop over the messages in all bags in time order, writing them to the output bag
  while (true) {
    auto next_message = read_next(readers);
    // Check if we've reached the end of all input bags or not
    if (next_message == std::nullopt) {
      std::cout << "\b\b\b\b100%\nProcessing complete\n";
      break;
    }
    // Write the message
    write_next_message(writer, next_message.value());
    std::cout << "\b\b\b\b  " << static_cast<uint64_t>(processed_count / num_messages) * 100 <<
      "%";
  }

  // Close the bags

  return 0;
}
