#include <stdio.h>

int main()
{
    float f = 1.0;
    unsigned int* p = (unsigned int*)&f;
    float* kp = (float*)p;
    printf("p=%x f=%f %f\n", *p, f, *kp);
    return 0;
}
