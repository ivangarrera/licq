#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include <string>

char *
convert_to_utf8(const char *input_text, const char *input_enc = NULL);

std::string
s_convert_to_utf8(const char *input_text, const char *input_enc = NULL);

#endif
