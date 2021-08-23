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


import os.path
import re
import shutil
import subprocess
import sys
import tempfile


def correct_merged_bag_output():
    return re.compile(
        r"""
        \s*Topic:\s(?P<bag1>[/\w]+?)_topic_0\s+Data:\s0\s+Time\sstamp:\s0\n
        \s*Topic:\s(?P<bag2>[/\w]+?)_topic_0\s+Data:\s100\s+Time\sstamp:\s25\n
        \s*Topic:\s(?P<bag3>[/\w]+?)_topic_0\s+Data:\s1000\s+Time\sstamp:\s30\n
        \s*Topic:\s(?P=bag3)_topic_0\s+Data:\s1001\s+Time\sstamp:\s40\n
        \s*Topic:\s(?P=bag3)_topic_0\s+Data:\s1002\s+Time\sstamp:\s50\n
        \s*Topic:\s(?P=bag3)_topic_0\s+Data:\s1003\s+Time\sstamp:\s60\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s101\s+Time\sstamp:\s65\n
        \s*Topic:\s(?P=bag3)_topic_0\s+Data:\s1004\s+Time\sstamp:\s70\n
        \s*Topic:\s(?P=bag1)_topic_1\s+Data:\s1\s+Time\sstamp:\s100\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s102\s+Time\sstamp:\s105\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s103\s+Time\sstamp:\s145\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s104\s+Time\sstamp:\s185\n
        \s*Topic:\s(?P=bag1)_topic_0\s+Data:\s2\s+Time\sstamp:\s200\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s105\s+Time\sstamp:\s225\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s106\s+Time\sstamp:\s265\n
        \s*Topic:\s(?P=bag1)_topic_1\s+Data:\s3\s+Time\sstamp:\s300\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s107\s+Time\sstamp:\s305\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s108\s+Time\sstamp:\s345\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s109\s+Time\sstamp:\s385\n
        \s*Topic:\s(?P=bag1)_topic_0\s+Data:\s4\s+Time\sstamp:\s400\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s110\s+Time\sstamp:\s425\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s111\s+Time\sstamp:\s465\n
        \s*Topic:\s(?P=bag1)_topic_1\s+Data:\s5\s+Time\sstamp:\s500\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s112\s+Time\sstamp:\s505\n
        \s*Topic:\s(?P=bag2)_topic_1\s+Data:\s113\s+Time\sstamp:\s545\n
        \s*Topic:\s(?P=bag2)_topic_0\s+Data:\s114\s+Time\sstamp:\s585\n
        \s*Topic:\s(?P=bag1)_topic_0\s+Data:\s6\s+Time\sstamp:\s600\n
        \s*Topic:\s(?P=bag1)_topic_1\s+Data:\s7\s+Time\sstamp:\s700\n
        \s*Topic:\s(?P=bag1)_topic_0\s+Data:\s8\s+Time\sstamp:\s800\n
        \s*Topic:\s(?P=bag1)_topic_1\s+Data:\s9\s+Time\sstamp:\s900\n
        """,
        re.MULTILINE | re.VERBOSE)


def generate_input_bag(
        num_topics,
        num_samples,
        start_data,
        start_time_offset,
        time_increment,
        binary_path):
    bag_path = tempfile.mkdtemp()
    command = [
        os.path.join(binary_path, 'generate_test_bags'),
        str(num_topics),
        str(num_samples),
        str(start_data),
        str(start_time_offset),
        str(time_increment),
        bag_path]
    print(f'Generating input bag {bag_path}')
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        print(f'Failed to generate bag {bag_path}:\n{result.stderr}')
        sys.exit(1)
    return bag_path


def merge_bags(inputs, merged_name, binary_path):
    print(f'Merging bags {inputs} into {merged_name}')
    command = [os.path.join(binary_path, 'merge_bags')] + inputs + ['-o', merged_name]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        print(f'Failed to merge {inputs} into bag {merged_name}:\n'
              f'{result.stderr}')
        sys.exit(1)


def check_output(merged_bag_path, binary_path):
    print(f'Printing bag {merged_bag_path}')
    result = subprocess.run(
        [os.path.join(binary_path, 'print_bag'), merged_bag_path],
        capture_output=True,
        text=True)
    if result.returncode != 0:
        print(f'Failed to print bag {merged_bag_path}:\n{result.stderr}')
        sys.exit(1)
    print(result.stdout)
    re_result = correct_merged_bag_output().match(result.stdout)
    if not re_result:
        return 1
    return 0


def main():
    binary_path = sys.argv[1]

    input_bag_1 = generate_input_bag(2, 10, 0, 0, 100, binary_path)
    input_bag_2 = generate_input_bag(2, 15, 100, 25, 40, binary_path)
    input_bag_3 = generate_input_bag(1, 5, 1000, 30, 10, binary_path)

    merged_bag = os.path.join(tempfile.gettempdir(), 'merged')
    merge_bags(
        [input_bag_1, input_bag_2, input_bag_3],
        merged_bag,
        binary_path)

    shutil.rmtree(input_bag_1)
    shutil.rmtree(input_bag_2)
    shutil.rmtree(input_bag_3)

    result = check_output(merged_bag, binary_path)

    shutil.rmtree(merged_bag)

    return result


if __name__ == '__main__':
    sys.exit(main())
