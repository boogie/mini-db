#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MAX_COLUMNS 64
#define MAX_COLUMN_NAME 100
#define MAX_STRING_LEN 100

enum {
    DATA_TYPE_STRING = 0x01,
    DATA_TYPE_INT8 = 0x11,
    DATA_TYPE_UINT8 = 0x12,
    DATA_TYPE_INT16 = 0x13,
    DATA_TYPE_INT32 = 0x14,
    DATA_TYPE_FLOAT64 = 0x21,
};
typedef struct {
    uint8_t type;
    char name[MAX_COLUMN_NAME];
} Column;

static FILE *flash_open(const char *file_name, const char *mode) {
  return fopen(file_name, mode);
}

static void flash_close(FILE *file) {
  if (file)
    fclose(file);
}

static bool flash_exists(const char *file_name) {
  FILE *file = fopen(file_name, "r");
  if (file) {
    fclose(file);
    return true; // File exists
  }
  return false; // File does not exist
}

static int get_type_length(uint8_t type) {
    switch (type) {
        case DATA_TYPE_INT8:
        case DATA_TYPE_UINT8: return 1;
        case DATA_TYPE_INT16: return 2;
        case DATA_TYPE_INT32: return 4;
        case DATA_TYPE_FLOAT64: return 8;
        case DATA_TYPE_STRING: return MAX_STRING_LEN;
        default: return 0;
    }
}

static void log_error(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
}

static int read_schema(FILE *file, Column *columns) {
    uint8_t version;
    fread(&version, 1, 1, file);

    uint8_t column_count;
    fread(&column_count, 1, 1, file);

    for (int i = 0; i < column_count; i++) {
        fread(&columns[i].type, 1, 1, file);
        int name_len = 0;
        char c;
        while (fread(&c, 1, 1, file) == 1 && c != 0 && name_len < MAX_COLUMN_NAME - 1) {
            columns[i].name[name_len++] = c;
        }
        columns[i].name[name_len] = '\0';
    }

    return column_count;
}

static void read_and_print_value(FILE *file, const char *name, uint8_t type) {
    printf("  %s: ", name);
    switch (type) {
        case DATA_TYPE_STRING: {
            char str[MAX_STRING_LEN];
            int len = 0;
            char c;
            while (fread(&c, 1, 1, file) == 1 && c != 0 && len < MAX_STRING_LEN - 1)
                str[len++] = c;
            str[len] = '\0';
            printf("%s\n", str);
            break;
        }
        default: {
            uint8_t buf[8] = {0};
            int len = get_type_length(type);
            fread(buf, 1, len, file);
            switch (type) {
                case DATA_TYPE_INT8:
                    printf("%d\n", *(int8_t *)buf); break;
                case DATA_TYPE_UINT8:
                    printf("%u\n", *(uint8_t *)buf); break;
                case DATA_TYPE_INT16:
                    printf("%d\n", *(int16_t *)buf); break;
                case DATA_TYPE_INT32:
                    printf("%d\n", *(int32_t *)buf); break;
                case DATA_TYPE_FLOAT64:
                    printf("%f\n", *(double *)buf); break;
            }
            break;
        }
    }
}

static bool value_matches(FILE *file, uint8_t type, const char *match) {
    char buffer[128];

    switch (type) {
        case DATA_TYPE_STRING: {
            char str[MAX_STRING_LEN];
            int len = 0;
            char c;
            while (fread(&c, 1, 1, file) == 1 && c != 0 && len < MAX_STRING_LEN - 1)
                str[len++] = c;
            str[len] = '\0';
            return strcmp(str, match) == 0;
        }
        default: {
            uint8_t buf[8] = {0};
            fread(buf, 1, get_type_length(type), file);
            switch (type) {
                case DATA_TYPE_INT8:
                    sprintf(buffer, "%d", *(int8_t *)buf); break;
                case DATA_TYPE_UINT8:
                    sprintf(buffer, "%u", *(uint8_t *)buf); break;
                case DATA_TYPE_INT16:
                    sprintf(buffer, "%d", *(int16_t *)buf); break;
                case DATA_TYPE_INT32:
                    sprintf(buffer, "%d", *(int32_t *)buf); break;
                case DATA_TYPE_FLOAT64:
                    sprintf(buffer, "%f", *(double *)buf); break;
            }
            return strcmp(buffer, match) == 0;
        }
    }
}

static void skip_value(FILE *file, uint8_t type) {
    if (type == DATA_TYPE_STRING) {
        char c;
        while (fread(&c, 1, 1, file) == 1 && c != 0); // skip until null terminator
    } else {
        fseek(file, get_type_length(type), SEEK_CUR);
    }
}

bool get_row_by_index(const char *filename, int target_row) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        log_error("Could not open file");
        return false;
    }

    Column columns[MAX_COLUMNS];
    int column_count = read_schema(file, columns);
    int current_row = 0;

    while (!feof(file)) {
        if (current_row == target_row) {
            printf("Row %d:\n", target_row);
            for (int i = 0; i < column_count; i++) {
                read_and_print_value(file, columns[i].name, columns[i].type);
            }
            fclose(file);
            return true;
        } else {
            for (int i = 0; i < column_count; i++) {
                skip_value(file, columns[i].type);
            }
            current_row++;
        }
    }

    fclose(file);
    log_error("Row not found");
    return false;
}

bool get_row_by_value(const char *filename, const char *column_name, const char *match_value) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        log_error("Could not open file");
        return false;
    }

    Column columns[MAX_COLUMNS];
    int column_count = read_schema(file, columns);

    int match_index = -1;
    for (int i = 0; i < column_count; i++) {
        if (strcmp(columns[i].name, column_name) == 0) {
            match_index = i;
            break;
        }
    }

    if (match_index == -1) {
        fclose(file);
        log_error("Column not found");
        return false;
    }

    int current_row = 0;
    while (!feof(file)) {
        long row_start = ftell(file);
        bool is_match = false;

        for (int i = 0; i < column_count; i++) {
            if (i == match_index) {
                is_match = value_matches(file, columns[i].type, match_value);
            } else {
                skip_value(file, columns[i].type);
            }
        }

        if (is_match) {
            fseek(file, row_start, SEEK_SET);
            printf("Row %d (match):\n", current_row);
            for (int i = 0; i < column_count; i++) {
                read_and_print_value(file, columns[i].name, columns[i].type);
            }
            fclose(file);
            return true;
        }

        current_row++;
    }

    fclose(file);
    log_error("Matching row not found");
    return false;
}

int main() {
  get_row_by_index("data.mdb", 1); // Print the 3rd row
  get_row_by_value("data.mdb", "email", "bob@chicago.com"); // Find row with email == ...
  return 0;
}
