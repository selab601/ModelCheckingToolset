SRCDIR:=$(CURDIR)/src/
BINDIR:=$(CURDIR)/bin/

BOOSTUSERNAME:=StateManager VerifierGenerator AttributeTableGenerator RuleSeparater DBMerge
NEO4JUSERNAME:=SCCFinder
BOTHUSERNAME:=StateFinder

LBOOST:=-lboost_filesystem -lboost_system
LNEO4J:=-lneo4j-client -lssl -lcrypto

BOOSTTARGET:=$(addprefix $(BINDIR),$(BOOSTUSERNAME))
NEO4JTARGET:=$(addprefix $(BINDIR),$(NEO4JUSERNAME))
BOTHTARGET:=$(addprefix $(BINDIR),$(BOTHUSERNAME))

all: $(BOOSTTARGET) $(NEO4JTARGET) $(BOTHTARGET)

$(BOOSTTARGET):$(BINDIR)%:$(addprefix $(SRCDIR),%.cpp)
		@echo Generating $(notdir $@)...
		@clang++ -std=c++1y $(LBOOST) $(addprefix $(SRCDIR),$(notdir $@).cpp) -o $@

$(NEO4JTARGET):$(BINDIR)%:$(addprefix $(SRCDIR),%.cpp)
		@echo Generating $(notdir $@)...
		@clang++ -std=c++1y $(LNEO4J) $(addprefix $(SRCDIR),$(notdir $@).cpp) -o $@

$(BOTHTARGET):$(BINDIR)%:$(addprefix $(SRCDIR),%.cpp)
		@echo Generating $(notdir $@)...
		@clang++ -std=c++1y $(LBOOST) $(LNEO4J) $(addprefix $(SRCDIR),$(notdir $@).cpp) -o $@

# $(GPPTARGET):$(BINDIR)%:$(addprefix $(SRCDIR),%.cpp)
# 		@echo Generating $(notdir $@)...
# 		@g++ -std=c++1y $(addprefix $(SRCDIR),$(notdir $@).cpp) -o $@
.PHONY : all clean

clean:
		@rm $(wildcard $(CURDIR)/bin/*) $(UNAME)