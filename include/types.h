#ifndef MCSOS_TYPES_H
#define MCSOS_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#ifndef __bool_true_false_are_defined
typedef int bool;
#define true 1
#define false 0
#endif
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif
