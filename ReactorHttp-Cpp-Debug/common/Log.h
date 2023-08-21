#pragma once
#include <stdarg.h>
#include <stdio.h>

#define  DEBUG   1  

#if DEBUG
#define LOG(type, fmt, ...) \
do {\
 printf(type "[%s : %s : %d] ", __FILE__, __FUNCTION__, __LINE__);\
 printf(fmt, ##__VA_ARGS__);\
 printf("\n\n");\
 } while(0)
#define Debug(fmt, ...) LOG("DEBUG: ", fmt, ##__VA_ARGS__)
#define Error(fmt, ...) do {LOG("ERROR: ", fmt, ##__VA_ARGS__); exit(0); } while(0)

#else
#define LOG(fmt, args...)  
#define Debug(fmt, args...) 
#define Error(fmt, args...)
#endif


/*
*  如果不加 do ... while(0) 在进行条件判断的时候(只有一句话), 省略了{}, 就会出现语法错误
*  if 
*     xxxxx
*  else
*     xxxxx
*  宏被替换之后, 在 else 前面会出现一个 ;  --> 语法错误
*/
