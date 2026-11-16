# Hospital Management System Makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude -O2
LDFLAGS = -lcrypt32

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
DB_DIR = database

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/config.c \
       $(SRC_DIR)/database.c \
       $(SRC_DIR)/auth.c \
       $(SRC_DIR)/session.c \
       $(SRC_DIR)/validation.c \
       $(SRC_DIR)/patient.c \
       $(SRC_DIR)/doctor.c \
       $(SRC_DIR)/appointment.c \
       $(SRC_DIR)/medical_records.c \
       $(SRC_DIR)/prescription.c \
       $(SRC_DIR)/pharmacy.c \
       $(SRC_DIR)/billing.c \
       $(SRC_DIR)/emergency.c \
       $(SRC_DIR)/reports.c \
       $(SRC_DIR)/json_utils.c \
       $(SRC_DIR)/response.c \
       $(SRC_DIR)/audit.c \
       $(SRC_DIR)/cJSON.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
SQLITE_OBJ = $(BUILD_DIR)/sqlite3.o

TARGET = $(BUILD_DIR)/hospital_api.cgi
TARGET_EXE = $(BUILD_DIR)/hospital_api.exe

.PHONY: all clean init-db run test

all: $(TARGET_EXE) $(TARGET)

$(SQLITE_OBJ): $(SRC_DIR)/sqlite3.c $(INC_DIR)/sqlite3.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_EXE): $(OBJS) $(SQLITE_OBJ)
	$(CC) $(OBJS) $(SQLITE_OBJ) $(LDFLAGS) -o $@

$(TARGET): $(TARGET_EXE)
	cp $(TARGET_EXE) $(TARGET)

init-db:
	python -c "import sqlite3; conn = sqlite3.connect('$(DB_DIR)/hospital.db'); conn.executescript(open('$(DB_DIR)/schema.sql').read()); conn.executescript(open('$(DB_DIR)/seed.sql').read()); conn.commit(); conn.close(); print('Database initialized and seeded!')"

test: $(TARGET_EXE)
	$(TARGET_EXE) --test
	python tests/test_runner.py

run: $(TARGET_EXE)
	python run_server.py

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*.exe $(BUILD_DIR)/*.cgi

