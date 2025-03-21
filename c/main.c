#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

/* OPTIMIZATION IDEAS:
    - Use less buffers, maybe allocate their sizes dynamically
    - Use (function) pointer structures instead of the if typecheck structures
*/

/* TODO:
    - Write the hex_to_data function (strings terminate with '00', numbers
   stored according to their value (int8 = 1 byte)
    - Change categorize_and_convert so that it reads numbers according to their
   data type
    - Error handling
*/

const char *DATA_TYPE_STRING = "01";
const char *DATA_TYPE_INT8 = "11";
const char *DATA_TYPE_UINT8 = "12";
const char *DATA_TYPE_INT16 = "13";
const char *DATA_TYPE_INT32 = "14";
const char *DATA_TYPE_FLOAT64 = "21";

int column_number = 0;
char *column_types[BUFFER_SIZE];

FILE *flash_open(const char *file_name, const char *mode) {
  return fopen(file_name, mode);
}

void flash_close(FILE *file) {
  if (file)
    fclose(file);
}

int flash_exists(const char *file_name) {
  FILE *file = fopen(file_name, "r");
  if (file) {
    fclose(file);
    return 1; // File exists
  }
  return 0; // File does not exist
}

char *flash_read_line(FILE *file, int line_number, char *buffer,
                      size_t buffer_size) {
  int current_line = 0;
  while (fgets(buffer, buffer_size, file)) {
    if (current_line == line_number) {
      return buffer;
    }
    current_line++;
  }
  return NULL; // Line not found
}

char *flash_read_column(FILE *file, int column_number, char *buffer,
                        size_t buffer_size) {
  char temp_line_buffer[BUFFER_SIZE];
  buffer[0] = '\0'; // Ensure buffer starts empty

  while (fgets(temp_line_buffer, sizeof(temp_line_buffer), file)) {
    char *token;
    char *temp = strdup(
        temp_line_buffer); // Duplicate line to prevent modification issues
    int current_column = 0;

    token = strtok(temp, ",\n"); // Tokenize first column
    while (token) {
      if (current_column == column_number) {
        if (strlen(buffer) + strlen(token) + 2 <
            buffer_size) { // Ensure enough space
          strcat(buffer, token);
          strcat(buffer, "\n"); // Add newline for readability
        }
        break;
      }
      token = strtok(NULL, ",\n");
      current_column++;
    }
    free(temp); // Free duplicate buffer
  }
  return buffer;
}

// Function to parse CSV and return key-value pairs
void read_line(const char *file_name, int x) {
  if (!flash_exists(file_name)) {
    printf("Error: File does not exist.\n");
    return;
  }

  FILE *file = flash_open(file_name, "r");
  if (!file) {
    printf("Error: Unable to open file.\n");
    return;
  }

  char buffer[BUFFER_SIZE];
  char header[BUFFER_SIZE];

  // Read the first line (header)
  if (!fgets(header, sizeof(header), file)) {
    printf("Error: Empty file.\n");
    flash_close(file);
    return;
  }

  // Read the requested line
  if (!flash_read_line(file, x, buffer, sizeof(buffer))) {
    printf("Error: Line %d not found.\n", x);
    flash_close(file);
    return;
  }

  // Tokenize the header and data lines
  char *headers[50], *values[50];
  int i = 0;

  headers[i] = strtok(header, ",\n");
  while (headers[i] && i < 50) {
    i++;
    headers[i] = strtok(NULL, ",\n");
  }

  i = 0;
  values[i] = strtok(buffer, ",\n");
  while (values[i] && i < 50) {
    i++;
    values[i] = strtok(NULL, ",\n");
  }

  // Print key-value pairs
  printf("Key-Value Pairs from Line %d:\n", x);
  for (int j = 0; headers[j] && values[j]; j++) {
    printf("%s: %s\n", headers[j], values[j]);
  }

  flash_close(file);
}

// Function to parse CSV and return key-value pairs
void read_column(const char *file_name, const char *column_name) {
  if (!flash_exists(file_name)) {
    printf("Error: File does not exist.\n");
    return;
  }

  FILE *file = flash_open(file_name, "r");
  if (!file) {
    printf("Error: Unable to open file.\n");
    return;
  }

  char buffer[BUFFER_SIZE];
  char header[BUFFER_SIZE];

  // Read the first line (header)
  if (!fgets(header, sizeof(header), file)) {
    printf("Error: Empty file.\n");
    flash_close(file);
    return;
  }

  // Tokenize the header
  char *headers[50];
  int i = 0, column_index = -1;

  headers[i] = strtok(header, ",\n");
  while (headers[i] && i < 49) {
    if (strcmp(headers[i], column_name) == 0) {
      column_index = i;
      break; // Stop searching once found
    }
    i++;
    headers[i] = strtok(NULL, ",\n");
  }

  // If column was not found, exit
  if (column_index == -1) {
    printf("Error: Column '%s' not found.\n", column_name);
    flash_close(file);
    return;
  }

  // Read the specified column
  if (!flash_read_column(file, column_index, buffer, sizeof(buffer))) {
    printf("Error: Unable to read column %d.\n", column_index);
    flash_close(file);
    return;
  }

  // Print extracted values
  printf("Values under column: %s\n", headers[column_index]);

  char *token = strtok(buffer, "\n");
  int row = 1;
  while (token) {
    printf("%d: %s\n", row++, token);
    token = strtok(NULL, "\n");
  }

  flash_close(file);
}

// CONVERTERS

// 1 BYTE
void hex_to_int8(const char *hex, char *output) {
  sprintf(output, "%hhd", (uint8_t)strtol(hex, NULL, 16));
}

// 1 BYTE
void hex_to_uint8(const char *hex, char *output) {
  sprintf(output, "%hhu", (uint8_t)strtol(hex, NULL, 16));
}

// 2 BYTES
void hex_to_int16(const char *hex, char *output) {
  sprintf(output, "%hd", (uint8_t)strtol(hex, NULL, 16));
}

// 3 BYTES
void hex_to_int32(const char *hex, char *output) {
  sprintf(output, "%d", (uint8_t)strtol(hex, NULL, 16));
}

// 8 BYTES
void hex_to_float64(const char *hex, char *output) {
  sprintf(output, "%lf", (uint8_t)strtol(hex, NULL, 16));
}

// N BYTES + 2 BYTES
void hex_to_string(const char *hex, char *output) {
  size_t len = strlen(hex);
  for (size_t i = 0; i < len; i += 2) {
    sscanf(hex + i, "%2hhX", &output[i / 2]);
  }
  output[len / 2] = '\0';
}

void uint8_to_hex(uint8_t num, char *hex_str, int size) {
  snprintf(hex_str, size, "%02X", num); // Ensure 2-digit hex format
  return;
}

int categorize_and_convert(char *unconverted, char *converted,
                           int column_index) { // TODO: more converters
  if (strcmp(column_types[column_index], DATA_TYPE_STRING) == 0) {
    hex_to_string(unconverted, converted);
  } else if (strcmp(column_types[column_index], DATA_TYPE_INT8) == 0) {
    hex_to_int8(unconverted, converted);
  } else if (strcmp(column_types[column_index], DATA_TYPE_UINT8) == 0) {
    hex_to_uint8(unconverted, converted);
  } else if (strcmp(column_types[column_index], DATA_TYPE_INT16) == 0) {
    hex_to_int16(unconverted, converted);
  } else if (strcmp(column_types[column_index], DATA_TYPE_INT32) == 0) {
    hex_to_int32(unconverted, converted);
  } else if (strcmp(column_types[column_index], DATA_TYPE_FLOAT64) == 0) {
    hex_to_float64(unconverted, converted);
  } else {
    return 1;
  }
  return 0;
}

// FUNCTIONS FOR READING THE .CSV FILE

// Version conversion
void hex_to_version(char *buffer, char *buffer_out) { // TODO - error handling
  char *version = strtok(buffer, " ");
  int version_converted = atoi(version);
  sprintf(buffer_out, "v%.1f", version_converted / 10.0);
  strcat(buffer_out, "\n");
  return;
}

// Header conversion, outputs header types
char *hex_to_header_types(char *buffer,
                          char *buffer_out) { // TODO - error handling
  char *type = strtok(NULL, " ");
  int column_index = 0;

  while (type != NULL) {
    if (strcmp(type, DATA_TYPE_STRING) == 0) {
      strcat(buffer_out, "String");
    } else if (strcmp(type, DATA_TYPE_INT8) == 0) {
      strcat(buffer_out, "Int8");
    } else if (strcmp(type, DATA_TYPE_UINT8) == 0) {
      strcat(buffer_out, "Uint8");
    } else if (strcmp(type, DATA_TYPE_INT16) == 0) {
      strcat(buffer_out, "Int16");
    } else if (strcmp(type, DATA_TYPE_INT32) == 0) {
      strcat(buffer_out, "Int32");
    } else if (strcmp(type, DATA_TYPE_FLOAT64) == 0) {
      strcat(buffer_out, "Float64");
    } else { // End of header, remove last " ", add newline
      buffer_out[strnlen(buffer_out, BUFFER_SIZE) - 1] = '\0';
      strcat(buffer_out, "\n");
      return type;
    }
    column_types[column_index] = type;
    column_index++;
    strcat(buffer_out, " ");
    type = strtok(NULL, " ");
    column_number++;
  }
  column_number = -1;
  return NULL;
}

// Header conversion, outputs header names
char *hex_to_header_names(char *buffer, char *buffer_out,
                          char *last_token) { // TODO - error handling
  char buffer_temp[BUFFER_SIZE];
  char buffer_temp_converted[BUFFER_SIZE];
  char *token = last_token;
  strcat(buffer_temp, last_token); // Write last token
  for (int i = 0; i < column_number; i++) {
    token = strtok(NULL, " ");
    while (token != NULL) {           // Until EOF
      if (strcmp(token, "00") == 0) { // Finished reading a header name, copy
                                      // and clear temporary buffer
        hex_to_string(buffer_temp, buffer_temp_converted);
        strcat(buffer_temp_converted, " ");
        strcat(buffer_out, buffer_temp_converted);
        memset(buffer_temp, 0, sizeof(buffer_temp));
        memset(buffer_temp_converted, 0, sizeof(buffer_temp_converted));
        break;
      } else {
        strcat(buffer_temp, token);
      }
      token = strtok(NULL, " ");
    }
  }

  buffer_out[strnlen(buffer_out, BUFFER_SIZE) - 1] = '\0';
  strcat(buffer_out, "\n");
  return last_token;
}

void hex_to_data(char *buffer, char *buffer_out,
                 char *last_token) { // TODO - error handling
}

// Process the entire .csv datafile
int8_t process_csv_file() {
  FILE *input_file = fopen("data.csv", "r");
  FILE *output_file = fopen("output.txt", "w");
  char buffer[BUFFER_SIZE];
  char buffer_out[BUFFER_SIZE];
  char *last_token = NULL;

  if (!input_file || !output_file) {
    perror("Error opening file");
    return 1;
  }
  fgets(buffer, sizeof(buffer), input_file);

  // Read and print version
  hex_to_version(buffer, buffer_out);

  // Read column types + quantity, returns last checked token that did not meet
  // the criteria
  last_token = hex_to_header_types(buffer, buffer_out);

  // Read column names
  last_token = hex_to_header_names(buffer, buffer_out, last_token);

  // Read data - TODO
  hex_to_data(buffer, buffer_out, last_token);

  // Print output
  printf("%s", buffer_out);

  fclose(input_file);
  fclose(output_file);
  return 0;
}

int main() {

  // read_line(file_name, 2);
  // read_column(file_name, "Name");

  process_csv_file();

  return 0;
}
