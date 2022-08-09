// A utility to test whether the patch was successful or not
#include <stdio.h>
#include <stdlib.h>

int main () {
	uint64_t timer_val = 0;
	printf("If your patch was successful, the following will not crash:\n");
	asm volatile(
		"mrs %[timer_val], S3_2_c15_c0_0"
		: [timer_val]"=r"(timer_val)
	);

	printf("Seems like it worked (Timer is %lld)!\n", timer_val);
}
