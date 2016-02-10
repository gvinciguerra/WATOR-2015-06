# ==========================================#
#   Wator, progetto di Laboratorio di SO    #
#   Copyright (c) 2015 Giorgio Vinciguerra  #
# ==========================================#

UNITY_ROOT=unity_framework

# Questo script di Unity in ruby, genera un test runner (contenente un main) per
# eseguire il test. Il primo parametro è il file contenente le funzioni di test,
# il secondo è la destinazione del test runner.
# È necessario che le funzioni di test, inizino con "test_".
GENERATE_RUNNER_SCRIPT=$(UNITY_ROOT)/auto/generate_test_runner.rb

C_COMPILER=gcc
CFLAGS=-std=c99 -Wall -Wextra

TARGET1=test1
SRC_FILES=$(UNITY_ROOT)/unity.c ../wator.c test_wator.c test_runners/test_wator_runner.c
INC_DIRS=-I../ -I$(UNITY_ROOT)
SYMBOLS=-DTEST

.PHONY: clean all default

all: clean default

default:
	ruby $(GENERATE_RUNNER_SCRIPT) test_wator.c  test_runners/test_wator_runner.c
	$(C_COMPILER) $(CFLAGS) $(INC_DIRS) $(SYMBOLS) $(SRC_FILES) -o $(TARGET1)
	./$(TARGET1)

clean:
	rm -f $(TARGET1)
