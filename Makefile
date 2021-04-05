SRC = CQKC_D_34_40_45
TXT = $(SRC).txt
ABS = $(SRC).abs

#OPTS += --def 11/04
#OPTS += --def 11/20
#OPTS += --def 11/34

all: $(ABS)

$(ABS): Makefile $(TXT) txt2abs
	txt2abs $(OPTS) --in $(TXT) --out $(ABS)

debug: Makefile $(TXT) txt2abs
	txt2abs --list $(OPTS) --in $(TXT) --out $(ABS)

txt2abs: txt2abs.cpp
	gcc txt2abs.cpp -o txt2abs

clean:
	rm -f txt2abs $(ABS)
