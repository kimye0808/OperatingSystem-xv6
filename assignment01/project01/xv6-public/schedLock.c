#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    __asm__("int $129");
    return 0;
}