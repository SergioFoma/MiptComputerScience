#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

int ConvertToNum(char* num_str);

int ParseArray(char** array, size_t len, int num);

int ParseLine(char* line, int num);

char** ReadLines(size_t* len);

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Not enough arguments!\n");
        return 0;
    }

    int num = ConvertToNum(argv[1]);

    if (num == -1) {
        printf("Number is not correct!\n");
        return 0;
    }

    size_t len = 0;
    char** array = ReadLines(&len);
    if (!array) {
        printf("Error of reading lines!\n");
        return 0;
    }
    
    int error_code = ParseArray(array, len, num);
    if (error_code != 0) {
        printf("Error of parse string arrat!\n");
        return 0;
    }

    return 0;
}

char** ReadLines(size_t* len) {
    assert(len);
    
    size_t index = 0;
    size_t capacity = 10;
    char** array = (char**)calloc(capacity, sizeof(char*));

    char* line = NULL;
    size_t line_capacity = 0;
    ssize_t line_len = 0;
    while ((line_len = getline(&line, &line_capacity, stdin)) != -1) {
        if (line_len == -1) {
            printf("Error of reading!\n");
            return NULL;
        }

        if (line_len == 3 && line[0] == '!' && line[1] == '!') {
            break;
        } else if (line_len > 2 && line[line_len - 3] == '!' && line[line_len - 2] == '!') {
            break;
        }
        
        line[line_len - 1] = '\0';
        if (index >= capacity) {
            capacity *= 20;
            array = (char**)realloc(array, capacity * sizeof(char*));
        }
        array[index++] = strdup(line);
    }

    if (line) free(line);
    *len = index;
    
    return array;
}

int ConvertToNum(char* num_str) {
    assert(num_str);

    int res = 0;
    size_t index = 0;

    while (num_str[index] != '\0') {
        if (index == 0 && num_str[index] == '-') return -1;

        res = res * 10 + (num_str[index] - '0');
        ++index;
    }

    return res;
}

int ParseArray(char** array, size_t len, int num) {
    assert(array);
    
    int error_code = 0;
    for (size_t index = 0; index < len && !error_code; ++index) {
        error_code = ParseLine(array[index], num);
        if (!error_code) {
            printf("%s\n", array[index]);
        }
    }

    return error_code;
}

int ParseLine(char* line, int num) {
    assert(line);

    int error_code = 0;
    size_t len = strlen(line);
    
    int word_count = 1;
    for (size_t index = 0; index < len; ++index) {
        if (line[index] == ' ') ++word_count;
    }

    for (size_t index = 0; index < len / 2; ++index) {
        char tmp = line[index];
        line[index] = line[len - index - 1];
        line[len - index - 1] = tmp;
    }
    
    num = word_count - num;
    char* start = line;
    char* end = start;
    size_t word_len = 0;
    int space_count = 0, flag = 1;
    while (flag) {
        end = strchr(start, ' ');
        ++space_count;

        if (space_count == num && end) {
            start = end + 1;
            continue;
        } else if (space_count == num) {
            break;
        }
        
        if (!end) {
            end = line + len;
            flag = 0;
        }
        word_len = (size_t)(end - start);
        for (size_t index = 0; index < word_len / 2; ++index) {
            char tmp = start[index];
            start[index] = start[word_len - 1 - index];
            start[word_len - 1 - index] = tmp;
        }

        start = end + 1;
    }

    return error_code;
}


