SILENCE = @

#--- Inputs ----#
COMPONENT_NAME = evdsptc
CPPUTEST_HOME = ./ext/cpputest

CPPUTEST_USE_EXTENSIONS = Y
CPPUTEST_USE_MEM_LEAK_DETECTION = Y
CPPUTEST_USE_GCOV = Y

CPP_PLATFORM = Gcc

SRC_DIRS = \
	../src

TEST_SRC_DIRS = \
	./src

INCLUDE_DIRS =\
  ../src\
  $(CPPUTEST_HOME)/include\

CPPUTEST_LDFLAGS += -lpthread

include $(CPPUTEST_HOME)/build/MakefileWorker.mk
