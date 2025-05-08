# Disassembly of `enable_dc_mva_ops`
On Apple SoCs, access to the data cache operations is controlled by an implementation defined register that seems to change for various chips.
`SCTLR_EL1.UCI` needs to first be set to 1, and it is helpful if `SCTLR_EL1.DZE` is 1 as well- by default both of these are enabled (in [`SCTLR_EL1_REQUIRED`](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/osfmk/arm64/proc_reg.h#L702)).
Despite `UCI` and `DZE` being enabled, you actually normally can't flush the data cache in EL0, as this is controlled by yet another MSR.

[`enable_dc_mva_ops`](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/osfmk/arm64/caches_asm.s#L181) is an XNU function that flips this special MSR and actually enables DC maintenance instructions.
Warning: The open-source macro linked here is not always correct for every CPU!
Different CPUs have this bit set in different registers.

The KDK kernel support libraries have the per-SoC definition for this method in each target's `caches_asm.o`, which can be found in the archive for your CPU in `${KDKROOT}/System/Library/KernelSupport/lib${SOC}.{VARIANT}.a`.
You can also find this method exported by the development kernel for your SoC, again in the KDK.

I am compiling a list of the different ways Apple SoC's enable DC ops here.

### M1-M3

T8103, T6000, T6031 do the following, making use of HID4/ EHID4 depending on whether the current core is a P or E core (respectively).
This follows the open-source kernel behavior.

```
00000000000000b4 <_enable_dc_mva_ops>:
      b4: d503237f     	pacibsp
      b8: a9bf7bfd     	stp	x29, x30, [sp, #-0x10]!
      bc: 910003fd     	mov	x29, sp
      c0: d5033fdf     	isb
      c4: d53800af     	mrs	x15, MPIDR_EL1
      c8: f27009ef     	ands	x15, x15, #0x70000
      cc: 9a9f17ef     	cset	x15, eq
      d0: b400006f     	cbz	x15, 0xdc <_enable_dc_mva_ops+0x28>
      d4: d538f42e     	mrs	x14, S3_0_C15_C4_1
      d8: 14000002     	b	0xe0 <_enable_dc_mva_ops+0x2c>
      dc: d538f40e     	mrs	x14, S3_0_C15_C4_0
      e0: 9274f9ce     	and	x14, x14, #0xfffffffffffff7ff
      e4: b400006f     	cbz	x15, 0xf0 <_enable_dc_mva_ops+0x3c>
      e8: d518f42e     	msr	S3_0_C15_C4_1, x14
      ec: 14000002     	b	0xf4 <_enable_dc_mva_ops+0x40>
      f0: d518f40e     	msr	S3_0_C15_C4_0, x14
      f4: d5033fdf     	isb
      f8: 910003bf     	mov	sp, x29
      fc: a8c17bfd     	ldp	x29, x30, [sp], #0x10
     100: d65f0fff     	retab
```

Enabling DC MVA ops requires setting bit 11 of (HID4/EHID4) to 0.

### M4

T8132 does this:

```
00000000000000cc <_enable_dc_mva_ops>:
      cc: d503237f     	pacibsp
      d0: a9bf7bfd     	stp	x29, x30, [sp, #-0x10]!
      d4: 910003fd     	mov	x29, sp
      d8: d5033fdf     	isb
      dc: d53cfc0f     	mrs	x15, S3_4_C15_C12_0
      e0: 927cf9ef     	and	x15, x15, #0xfffffffffffffff7
      e4: d51cfc0f     	msr	S3_4_C15_C12_0, x15
      e8: d5033fdf     	isb
      ec: 910003bf     	mov	sp, x29
      f0: a8c17bfd     	ldp	x29, x30, [sp], #0x10
      f4: d65f0fff     	retab
```

Enabling DC MVA ops requires setting bit 3 of ACFG_EL1 to 0.

According to [`H16.h`](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/pexpert/pexpert/arm64/H16.h#L54) which says "`DCache maintenance op disable controls in ACFG_EL1`", it seems that `S3_4_C15_C12_0` is this mysterious `ACFG_EL1` register.
It looks like multiple other families have this register, but only on H16 does it control the DC ops.
I am assuming that "H16" means M4.
