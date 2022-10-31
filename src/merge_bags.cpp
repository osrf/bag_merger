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

#include <rcpputils/filesystem_helper.hpp>

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

struct BagMergerOptions
{
  std::vector<rcpputils::fs::path> inputs;
  std::optional<rcpputils::fs::path> output{std::nullopt};
  uint max_bagfile_duration{0};
  uint max_bagfile_size{0};
};

using ReaderPtr = std::shared_ptr<rosbag2_cpp::Reader>;
using NextMessage = std::optional<std::shared_ptr<rosbag2_storage::SerializedBagMessage>>;
struct ReaderWithNext
{
  ReaderPtr reader;
  NextMessage next_message;
};
using ReaderStore = std::vector<ReaderWithNext>;

BagMergerOptions get_options(int argc, char ** argv)
{
  BagMergerOptions options;

  // There must be at least 4 arguments:
  // program name, -o, output destination input bag 1
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " -o <output bag> <input bag...> " <<
      "[-b <max_bagfile_size> -d <max_bagfile_duration>" << std::endl;
    return options;
  }

  std::string output_flag = "-o";
  std::string duration_flag = "-d";
  std::string size_flag = "-b";

  for (int ii = 1; ii < argc; ++ii) {
    if (
      (output_flag == argv[ii] || duration_flag == argv[ii] || size_flag == argv[ii]) &&
      ii + 1 == argc)
    {
      std::cerr << "Missing argument to flag " << argv[ii] << std::endl;
      return options;
    } else if (output_flag == argv[ii]) {
      options.output = rcpputils::fs::path(argv[ii + 1]);
      ii += 1;
    } else if (duration_flag == argv[ii]) {
      options.max_bagfile_duration = std::stoi(argv[ii + 1]);
      ii += 1;
    } else if (size_flag == argv[ii]) {
      options.max_bagfile_size = std::stoi(argv[ii + 1]);
      ii += 1;
    } else {
      options.inputs.push_back(rcpputils::fs::path(argv[ii]));
    }
  }
  return options;
}

ReaderStore make_readers(const std::vector<rcpputils::fs::path> & input_names)
{
  ReaderStore result;

  for (const auto & input_name : input_names) {
    std::unique_ptr<rosbag2_cpp::readers::SequentialReader> reader_impl =
      std::make_unique<rosbag2_cpp::readers::SequentialReader>();
    const rosbag2_cpp::StorageOptions storage_options({input_name.string(), "sqlite3"});
    const rosbag2_cpp::ConverterOptions converter_options(
      {rmw_get_serialization_format(), rmw_get_serialization_format()});
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

std::unique_ptr<rosbag2_cpp::Writer> make_writer(
  const rcpputils::fs::path & output_name, BagMergerOptions options)
{
  const rosbag2_cpp::StorageOptions storage_options(
    {output_name.string(), "sqlite3", options.max_bagfile_size, options.max_bagfile_duration});
  const rosbag2_cpp::ConverterOptions converter_options(
    {rmw_get_serialization_format(), rmw_get_serialization_format()});
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
        result.begin(), result.end(), [&t](auto topic) {return topic.name == t.name;});
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
  auto bag_merger_options = get_options(argc, argv);
  rcpputils::fs::path output;

  if (bag_merger_options.inputs.size() == 0) {
    std::cerr << "Missing input bags\n";
    return 1;
  }
  if (bag_merger_options.output == std::nullopt) {
    std::cerr << "Missing output bag name\n";
    return 1;
  }
  output = bag_merger_options.output.value();

  // Create a reader for each input bag
  ReaderStore readers = make_readers(bag_merger_options.inputs);
  if (readers.size() == 0) {
    return 1;
  }
  // Make the output directory absolute
  if (!output.is_absolute()) {
    auto cwd = rcpputils::fs::current_path();
    output = cwd / output;
  }
  // Create the output directory
  if (output.exists()) {
    std::cerr << "Output bag directory already exists\n";
    return 1;
  }
  std::cout << "Creating output directory '" << output.string() << "' for destination bag\n";
  if (!rcpputils::fs::create_directories(output)) {
    std::cerr << "Failed to create destination bag's output directory\n";
    return 1;
  }
  // Create a writer for the output bag
  std::unique_ptr<rosbag2_cpp::Writer> writer = make_writer(output, bag_merger_options);

  // Combine the input bag topics into one list and use it for the output bag metadata
  auto input_topics = combine_input_topics(readers);
  set_output_metadata(writer, input_topics);

  auto num_messages = get_total_message_count(readers);
  std::cout << "Processing " << num_messages << " messages from " << readers.size() <<
    " input bags\n";
  std::cout << std::unitbuf;
  std::cout << "   00%";
  uint64_t processed_count = 0;
  int processed_fraction = 0;
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
    processed_count += 1;
    processed_fraction =
      static_cast<float>(processed_count) / static_cast<float>(num_messages) * 100.0f;
    std::cout << "\b\b\b\b\b" << processed_fraction << "%";
  }

  // Close the bags

  return 0;
}
