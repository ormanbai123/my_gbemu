#ifndef GB_UTILS_H
#define GB_UTILS_H

//#define BIT_TEST(num,pos) (((num)>>(pos)) & 1)

#define BIT_SET(num, pos) ((num) |= (1 << (pos)))

#define BIT_CLEAR(num, pos) ((num) &= ~(1 << (pos)))

#define BIT_GET(num, pos) (((num) >> (pos)) & 1)

#endif // !UTILS_H