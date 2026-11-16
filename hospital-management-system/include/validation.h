#ifndef VALIDATION_H
#define VALIDATION_H

int validate_email(const char *email);
int validate_phone(const char *phone);
int validate_date(const char *date_str);
int validate_not_empty(const char *str);
void sanitize_text(char *dest, const char *src, int max_len);

#endif // VALIDATION_H

