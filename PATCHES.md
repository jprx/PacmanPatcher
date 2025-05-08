
# Patch Sets

Here's a brief overview of what the patches in `patch.c` do.

## Version String

We replace the version string `DEVELOPMENT_ARM64` with `PACMANPATCH_ARM64` as an easy way to confirm the patches are working.
You can make this whatever you like.

## PMCR0 Patch Set

XNU writes to PMCR0 with the following pattern:

```
08 80 86 52  mov   w8,#0x3400
08 e0 a0 72  movk  w8,#0x700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
```

We need bit 30 set, so we can just do this (1 byte patch):

```
08 80 86 52  mov   w8,#0x3400
08 e0 a8 72  movk  w8,#0x4700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
```

## ACFG Patch Set

Anytime DC ops are being disabled via `ACFG_EL1` (eg. on H16), XNU follows the following pattern:

```
0ffc3cd5   mrs     x15, acfg_el1
ef017db2   orr     x15, x15, #0x8
0ffc1cd5   msr     acfg_el1, x15
```

This happens in two spots- `disable_dc_mva_ops` and `CleanPoC_DcacheRegion_Force_nopreempt`.
We mask out bit 3 to make sure it's always zero (meaning DC MVA ops are always enabled).

```
0ffc3cd5   mrs     x15, acfg_el1
eff97c92   and     x15, x15, #0xfffffffffffffff7
0ffc1cd5   msr     acfg_el1, x15
```

## HID4/ EHID4 Patch Set 1

We want to disable all places where XNU writes a 1 to HID4/ EHID4 at bit 11.
This happens in two spots (`disable_dc_mva_ops` and `CleanPoC_DcacheRegion_Force_nopreempt`) and they both look like this:

```
2ef438d5   mrs     x14, s3_0_c15_c4_1
02000014   b       0xfffffe000734561c
0ef438d5   mrs     x14, hid4
ce0175b2   orr     x14, x14, #0x800
```

We replace the OR with an AND masking off bit 11:

```
2ef438d5   mrs     x14, s3_0_c15_c4_1
02000014   b       0xfffffe000734561c
0ef438d5   mrs     x14, hid4
cef97492   and     x14, x14, #0xfffffffffffff7ff
```

## HID/ EHID4 Patch Set 2
On older kernels, XNU used to write to HID4/ EHID4 slightly differently.
`x14` holds the value of EHID4/HID4 before calling `CleanPoC_DcacheRegion_internal`, and `x15` contains `mpidr_el1`.
This only happens in one spot, in `CleanPoC_DcacheRegion_Force_nopreempt`:

```
ce0175b2   orr     x14, x14, #0x800
6f0000b5   cbnz    x15, 0xfffffe00072d5600
2ef418d5   msr     s3_0_c15_c4_1, x14
02000014   b       0xfffffe00072d5604
0ef418d5   msr     hid4, x14
```

Just replace the `or` with an `and` like above:

```
cef97492   and     x14, x14, #0xfffffffffffff7ff
6f0000b5   cbnz    x15, 0xfffffe00072d5600
2ef418d5   msr     s3_0_c15_c4_1, x14
02000014   b       0xfffffe00072d5604
0ef418d5   msr     hid4, x14
```
