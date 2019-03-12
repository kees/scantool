#!/usr/bin/python
import sys

codes = set()

default = dict()
for line in open("codes-Default.txt"):
    line = line.strip()
    code, text = line.split('\t',1)
    default[code] = text
    codes.add(code)

generic = dict()
for line in open("codes-Generic.txt"):
    line = line.strip()
    code, text = line.split('\t',1)
    generic[code] = text
    codes.add(code)

progress = dict()
for line in open("progress.txt"):
    line = line.strip()
    code, text = line.split('\t',1)
    progress[code] = text

result = dict()
for code in codes:
    if default.get(code, None) == None:
        result[code] = generic[code]
        continue
    if generic.get(code, None) == None:
        result[code] = default[code]
        continue

    if len(generic[code]) <= len(default[code]):
        start = generic[code]
        full = default[code]
    else:
        start = default[code]
        full = generic[code]

    if full.startswith(start):
        result[code] = full
    else:
        result[code] = progress[code]
    #else:
    #    print "Conflict (%s):" % (code)
    #    print "\tDefault: %s" % (default[code])
    #    print "\tGeneric: %s" % (generic[code])
    #    #print "use what?"
    #    #result[code] = sys.stdin.readline().strip()
    #    #progress.write("%s\t%s\n" % (code, result[code]))

output = open("output.txt", "w")
for code in codes:
    output.write("%s\t%s\n" % (code, result[code]))
output.close()
