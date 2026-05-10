#pragma once

#include <stdint.h>

extern "C" {

double cotan(double x);
int64_t trunc_double(double x);
double rand_double(double x);
double rand_double_range(double a, double b);
int64_t rand_int64(int64_t x);
int64_t rand_int64_range(int64_t a, int64_t b);
int64_t div_qum(int64_t a, int64_t b);
int64_t mod_qum(int64_t a, int64_t b);
double fpow(double a, int n);

} // extern "C"
