# Bag merger

A tool for merging multiple rosbag2 bags into one.
All samples from all input bags will be present in the output bag.
They will be in time order, meaning overlapping input bags will be interleaved.

## Usage

The bag merger requires two or more input bags, and the name of one output bag.
The output bag name must be specified using the `-o` flag.

```
bag_merger -o <output_bag> <input_bag_1> <input_bag_2> [...input_bag_n]
```

The input bags must exist, or an error will occur.
The output bag must _not_ exist, or an error will occur.
