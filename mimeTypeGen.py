#!/bin/env python3
# Changed this to add or remove mime types you want or dont want

from sys import argv

if len(argv) < 2:
	print("Please provide a path to your mime.types file")
	exit(1)

with open(argv[1]) as infile:
	lines = infile.readlines()

print("""
typedef struct {
char const *const Extension;
char const *const Type;
} mimeType;

const mimeType mTypes[] = {
""")

for line in lines:
	if len(line) == 0 or line[0] == '#':
		continue

	words = line.split()
	if len(words) == 0:
		continue

	mimetype = words[0]
	extension = words[1]

	print(f"\t{{\"{extension}\", \"{mimetype}\\r\\n\"}},")

print("};")
