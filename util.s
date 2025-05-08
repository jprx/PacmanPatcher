
.global _time_load
_time_load:
	dsb sy
	isb
	mrs x9, S3_2_c15_c0_0
	isb
	ldr x10, [x0]
	isb
	mrs x11, S3_2_c15_c0_0
	isb
	dsb sy
	sub x0, x11, x9
	ret

.global _flushd
_flushd:
	dsb sy
	isb
	dc civac, x0
	isb
	dsb sy
	ret

.global _flushi
_flushi:
	dsb sy
	isb
	ic ivau, x0
	isb
	dsb sy
	ret
