TXT=$(wildcard *.txt)
OUT=$(TXT:.txt=.out)

all: codes.dat

# Inject origin column from public codes.
%.out: %.txt Makefile
	# Inject column 2 based on file name.
	set -e; \
	origin=$$(echo $< | cut -d- -f2 | cut -d. -f1); \
	awk -F"\t" '{out=$$1"\t'$$origin'"; for (i=2;i<=NF;i++) { out=out"\t"$$i }; print out}' $< > $@

# Build Allegro objects based on code prefix.
codes.dat: $(OUT)
	# Use only unique entries, without ;-prefixed comments.
	sort -u $^ | grep -v '^;' >all.out
	rm -f $@.new
	set -e; for code in $$(cut -c1 all.out | sort -u); do \
		lower=$$(echo $$code | tr A-Z a-z); \
		grep ^$$code all.out > $${lower}codes; \
		dat -a $@.new $${lower}codes; \
	done
	mv $@.new $@

clean:
	rm -rf *codes *.out codes.dat*
