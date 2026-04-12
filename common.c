#ifndef __COMMON_C__
#define __COMMON_C__

#define CLEAN(f) __attribute__((cleanup(f##Wrapper)))

#endif
