#!/usr/bin/env python3

# Copyright 2021, Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import os
import os.path
import subprocess
import sys
import tempfile


def generate_input_bag(
        num_topics,
        num_samples,
        start_data,
        start_time_offset,
        time_increment,
        binary_path):
    bag_path = tempfile.mkdtemp()
    print(f'Generating input bag {bag_path}')
    result = subprocess.run(
        [os.path.join(binary_path, 'generate_test_bags'),
         str(num_topics),
         str(num_samples),
         str(start_data),
         str(start_time_offset),
         str(time_increment),
         bag_path],
        capture_output=True,
        text=True)
    if result.returncode != 0:
        print('Failed to generate bag ' + bag_path)
        sys.exit(1)
    return bag_path


def merge_bags(inputs, merged_name, binary_path):
    command = [os.path.join(binary_path, 'merge_bags')] + inputs + [merged_name]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        print(f'Failed to merge {inputs} into bag {merged_name}')
        sys.exit(1)
    return merged_name


def check_output(merged_bag_path, binary_path):
    result = subprocess.run(
        [os.path.join(binary_path, 'print_bag'), merged_bag_path],
        capture_output=True,
        text=True)
    if result.returncode != 0:
        print('Failed to print bag ' + merged_bag_path)
        sys.exit(1)
    return 0


def main():
    binary_path = sys.argv[1]

    input_bag_1 = generate_input_bag(2, 10, 0, 0, 100, binary_path)
    input_bag_2 = generate_input_bag(2, 15, 100, 25, 40, binary_path)
    input_bag_3 = generate_input_bag(1, 5, 1000, 30, 10, binary_path)

    merged_bag = merge_bags(
        [input_bag_1, input_bag_2, input_bag_3],
        'merged',
        binary_path)

    os.remove(input_bag_1)
    os.remove(input_bag_2)
    os.remove(input_bag_3)

    result = check_output(merged_bag, binary_path)

    os.remove(merged_bag)

    return result


if __name__ == '__main__':
    sys.exit(main())
