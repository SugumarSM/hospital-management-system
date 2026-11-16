#include "validation.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

int validate_email(const char *email) {
    if (!email) return 0;
    size_t len = strlen(email);
    if (len < 5 || len > 120) return 0;
    const char *at = strchr(email, '@');
    if (!at || at == email) return 0;
    const char *dot = strrchr(at, '.');
    if (!dot || dot == at + 1 || *(dot + 1) == '\0') return 0;
    return 1;
}

int validate_phone(const char *phone) {
    if (!phone) return 0;
    int digit_count = 0;
    for (size_t i = 0; phone[i] != '\0'; i++) {
        char c = phone[i];
        if (isdigit((unsigned char)c)) {
            digit_count++;
        } else if (c != '+' && c != '-' && c != ' ' && c != '(' && c != ')') {
            return 0;
        }
    }
    return (digit_count >= 7 && digit_count <= 15);
}

int validate_date(const char *date_str) {
    if (!date_str || strlen(date_str) != 10) return 0;
    // Format: YYYY-MM-DD
    if (date_str[4] != '-' || date_str[7] != '-') return 0;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (!isdigit((unsigned char)date_str[i])) return 0;
    }
    int year = atoi(date_str);
    int month = atoi(date_str + 5);
    int day = atoi(date_str + 8);
    if (year < 1900 || year > 2100) return 0;
    if (month < 1 || month > 12) return 0;
    if (day < 1 || day > 31) return 0;
    return 1;
}

int validate_not_empty(const char *str) {
    if (!str) return 0;
    while (*str) {
        if (!isspace((unsigned char)*str)) return 1;
        str++;
    }
    return 0;
}

void sanitize_text(char *dest, const char *src, int max_len) {
    if (!dest || max_len <= 0) return;
    dest[0] = '\0';
    if (!src) return;
    int di = 0;
    for (int si = 0; src[si] != '\0' && di < max_len - 1; si++) {
        unsigned char c = (unsigned char)src[si];
        // Allow printable ASCII characters
        if (c >= 32 && c <= 126) {
            dest[di++] = (char)c;
        }
    }
    dest[di] = '\0';
}

