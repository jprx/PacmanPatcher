# PacmanPatcher
We want to turn on the Apple PMC registers (specifically acccess `PMC0` or `S3_2_c15_c0_0`) from userspace.

This is controlled by the following bits:

* `CNTKCTL_EL1.EL0PTEN` (bit 9) should be `1`
* `CNTKCTL_EL1.EL0VTEN` (bit 8) should be `1`
* `PMCR0_EL1` should be masked with `PMCR0_USEREN_EN` (bit 30 should be `1`)

References:
* `osfmk/arm64/monotonic_arm64.c`
* https://developer.arm.com/documentation/ddi0595/2020-12/AArch64-Registers/CNTKCTL-EL1--Counter-timer-Kernel-Control-register

We want to do this by patching the kernel itself, not compiling a new kernel. While it is possible to compile XNU and make these changes ourselves, we want to maintain all offsets such that any code reuse chains / hardcoded symbols will remain constant. Additionally, I have found the KDK development kernel to be more stable than the development kernel I built from scratch.

Solution is to patch `kernel.development.t8101` (Or whatever flavor your machine uses) such that all writes to `CNTKCTL_EL1` and `PMCR0_EL1` always set the correct bits.

I searched through the kernelcache in Ghidra for any writes to those MSRs and traced through the kernel binary/ XNU source to find where the MSRs are set. I then hand-coded patches for the MacOS 12.4 (`xnu-8020.121.3~4`) `development` kernel that ships with the KDK (`21F79`). These patches might work for future/ past versions too.

**Disclaimer**

These patches might cause system instability, security issues, or crashes. Additionally, they intentionally make your system less secure by enabling userspace access to the high resolution timers. Proceed at your own risk!

# Requirements
This set of patches has only been tested on a Mac Mini (`t8101`) and an M1 Pro MacBook Pro (`t6000`), both running MacOS 12.4 (`xnu-8020.121.3` build `21F79`), plus an M1 Pro MacBook Pro (`t6000`) running MacOS 12.5.1 (`xnu-8020.141.5~2` build `21G83`).

## How To Install

1. Make sure your Mac is running MacOS 12.5.1 `21G83`, which is the latest supported version. (You can verify this with `sw_vers`). (Other versions might work too). (If you want an older version, check the git tags to see if your version is supported. Or just remove the version check in `patch.c` if you're feeling brave).
1. Identify your machine architecture identifier by checking the version string with `uname -sra`. For example, if it contains `root:xnu-8020.121.3~4/RELEASE_ARM64_T8101`, then your machine is `t8101` (a Mac Mini).
1. Download the KDK for your MacOS version from the Apple Developer downloads section (you'll need an Apple ID but as of July 2022 you don't need a paid developer account) and install it.
1. Copy `/Library/Developer/KDKs/YOUR_KDK.kdk/System/Library/Kernels/kernel.development.YOUR_SOC` to a folder somewhere.
1. Build PacmanPatcher with `make` (inside of the `PacmanPatcher` directory).
1. Run PacmanPatcher with `./patch [kernel to patch]`. Note: the binary will be updated in-place!
1. Recompile the kernelcache and boot the new kernel (see [booting the patched kernel](#booting-the-patched-kernel))

Instruction TLDR (for MacOS 12.4 `t8101` machines aka Mac Mini, replace `t8101` with your arch version):

```
git clone git@github.com:jprx/PacmanPatcher.git
cd PacmanPatcher
make
cp /Library/Developer/KDKs/KDK_12.4_21F79.kdk/System/Library/Kernels/kernel.development.t8101 kernel.patched.t8101
./patch kernel.patched.t8101
kmutil create -a arm64e -z -v -s none -n boot -k `pwd`/kernel.patched.t8101 -B patched.kc -V development -x $(kmutil inspect -V release --no-header | awk '{print " -b "$1; }')
```

Then follow the instructions in [booting the patched kernel](#booting-the-patched-kernel).

## Booting the patched kernel

Once you've got the patched kernel binary, build the kernelcache with:

```
kmutil create -a arm64e -z -v -s none -n boot -k `pwd`/YOUR_PATCHED_KERNEL_MACHO -B patched.kc -V development -x $(kmutil inspect -V release --no-header | awk '{print " -b "$1; }')
```

Your resulting kernelcache will be called `patched.kc.development`.

Then, boot into 1TR recovery mode (shut down -> turn on and hold power button -> options) and open a Terminal (Utilities -> Terminal) and run:

```
kmutil configure-boot -v /Volumes/YOUR_VOLUME -c /Volumes/YOUR_VOLUME/PATH/TO/KERNELCACHE/patched.kc.development
```

and reboot.

(Your volume name is probably `MacintoshHD`).

Finally, open a terminal and run `uname -sra`. You should see the following:

```
...root:xnu-8020.121.3~4/PACMANPATCH_ARM64_T...
```

Give it a test by running `./test_patch`!

**Important Note**
Any kernel extensions you have installed will be compiled into the newly built kernelcache. So, if you have any kexts you will be updating soon (eg. if you are running [PacmanKit](https://github.com/jprx/PacmanKit)) make sure to leave those kexts out of the kernelcache build invocation.

# Patch Sets

Here's a brief overview of what the patches do. See `patch.c` for info on each one.

## Version String

We replace the version string `root:xnu-8020.141.5~2/DEVELOPMENT_ARM64_T` with `root:xnu-8020.141.5~2/PACMANPATCH_ARM64_T`. You can make this whatever you like.

By default, the patch utility will stop if it doesn't detect the development version of the MacOS 12.5.1 kernel (`xnu-8020.141.5~2`). You can comment that out if you want to try this on a different kernel version.

You can also check the git tags to see if your older MacOS version is supported by PacmanPatcher.

## CNTKCTL Patch Set

This sets bits 8 and 9 in `CNTKCTL_EL1`.

```
Original patterns look something like:

(some branch)
mrs x9,cntkctl_el1
orr x8,x9,x8, LSL #0x4
orr x8,x8,#0xf
msr cntkctl_el1,x8

We want bits 8 and 9 set too. Need another instruction
so we just overwrite the previous branch.

In all cases (as of MacOS 12.4) that this occurs, the proceeding inst
is a branch that leads to a kernel panic condition. We never will hit that
panic (in good code) so we can assume that branch never runs and just overwrite it.

Not ideal, but then again, we are editing the kernel by hand here...

mrs x9,cntkctl_el1
orr x8,x9,x8, LSL #0x4
orr x8,x8,#0xf
orr x8,x8,#0x3 LSL #0x8
msr cntkctl_el1,x8
```

## PMCR0 Patch Set 1

```
Sometimes we write to PMCR0 with the following pattern:
08 80 86 52  mov   w8,#0x3400
08 e0 a0 72  movk  w8,#0x700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8

We need bit 30 set, so we can just do this (1 byte patch):

08 80 86 52  mov   w8,#0x3400
08 e0 a8 72  movk  w8,#0x4700, LSL #16
08 f0 19 d5  msr   sreg(0x3, 0x1, c0xf, c0x0, 0x0),x8
```

## PMCR0 Patch Set 2

```
Sometimes XNU does the following, which eventually loads x19 into PMCR0:

This is the most dangerous patch we support as it doesn't explicitly include a
write to the register in question. Future versions MAY break this!

Proceed with caution here.

73 80 86 52 mov  w19,#0x3403
13 e0 a0 72 movk w19,#0x700, LSL #16

We need bit 30 set, so we can just do this:

73 80 86 52 mov  w19,#0x3403
08 e0 a8 72 movk w8,#0x4700, LSL #16
```
