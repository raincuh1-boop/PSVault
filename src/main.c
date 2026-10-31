#include <orbis/libkernel.h>

/* First milestone: bootable native package. UI and networking come next. */
int main(void) {
    for (;;) {
        sceKernelUsleep(1000000);
    }
    return 0;
}
