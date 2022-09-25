#!/bin/env python3
# Changed this to add or remove mime types you want or dont want

import sys

if len(sys.argv) < 2:
	print("Please provide a path to your mime.types file")
	exit(1)

infile = open(sys.argv[1])
fullfile = infile.readlines()
infile.close()

base = """
typedef struct {
char const *const Extension;
char const *const Type;
} mimeType;

const mimeType mTypes[] = {

"""

template = "\t{\"{1}\",\"{2}\\r\\n\"},"

print(base)



for line in fullfile:
	if len(line) == 0 or line[0] == '#':
		continue
	
	words = line.split()
	if len(words) == 0:
		continue

	type = line[0]
	extension = line[1]
	
	print(f"\t{{\"{extension}\", \"{type}\\r\\n\"}},")
print("};")
