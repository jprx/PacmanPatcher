# PacmanPatcher

PacmanPatcher patches XNU to enable two features in EL0:
- Access to the Apple performance counter PMC registers (specifically `PMC0` / `S3_2_c15_c0_0`).
- Ability to use data cache maintenance instructions (eg. the ability to use `dc civac`).

After patching your kernel, you'll be able to read the performance counters and flush the cache from EL0.

# Supported Systems
Latest tested XNU: `xnu-11417.121.6` / macOS 15.5 RC1 / 24F74

Tested configurations:

| SoC   | System         | Host OS         | Working? |
|-------|----------------|-----------------|----------|
| T8132 | M4 Mac Mini    | 15.1 (24B2083)  |    ✅    |
| T6000 | M1 MacBook Pro | 15.5 RC (24F74) |    ✅    |
| T8101 | M1 Mac Mini    | 12.4 (21F79)    |    ✅    |

# Usage

### Getting Files
1. Download the KDK matching your Mac from Apple.
   - Use `sw_vers` to get your version (eg. `24F74`).
1. Get your SOC with `uname -v`. It's the last part of the XNU version string.
   -  Example: `root:xnu-8020.121.3~4/RELEASE_ARM64_T8101`'s SOC is `T8101`.
1. Grab a copy of `/Library/Developer/KDKs/YOUR_KDK.kdk/System/Library/Kernels/kernel.development.YOUR_SOC`.
   - This is the kernel we'll be patching.

### Patching the Kernel
1. In this folder, `make`
1. `./patch [kernel]`
   - Note that the kernel will be patched in-place.
   - You can ignore most `"Warning: Found fewer hits than expected"` warnings, as not all patch sets apply to all SoCs (meaning some of the patch sets will have 0 matches in your kernel).
   - I would test the patches first and debug using the warnings later if things don't work on the first try.
1. Make a kernel collection with:

```
kmutil create -a arm64e -z -v -s none -n boot -k YOUR_KERNEL -B patched.kc -V development -x $(kmutil inspect -V release --no-header | awk '{print " -b "$1; }')
```

### Booting the Kernel
1. Shut down your Mac.
1. Boot it up while holding down the power button until you see the options menu.
   - This is called "one true recovery" or 1TR.
1. Select options, then go to Utilities -> Terminal.
1. If using FileVault, unlock your disk with `diskutil apfs unlockVolume diskXsY`, where X and Y are replaced with the identifiers of your disk (probably `disk3s5`)
   - You can do this via the GUI as well by going to Utilities -> Startup Security Utility and clicking the unlock button.
1. Tell iBoot to load your kernel collection with: `kmutil configure-boot -v /Volumes/YOUR_VOLUME -c /Volumes/YOUR_VOLUME/PATH/TO/KERNELCACHE/patched.kc.development`

### Checking if it Worked
1. You should be able to restart your Mac and boot it like normal.
1. Run `uname -v`, you should see something like `root:xnu-8020.121.3~4/PACMANPATCH_ARM64_T...`
1. Use `./test_pmc` and `./test_flush` to confirm the patches are working.

### Reverting to Normal
1. Boot into 1TR again (turn on while holding the power button, then "Options").
1. Go to Utilities -> Startup Security Utility and select "Full Security".
1. Reboot.

**Note**: If your kernel fails to boot, you'll end up in a recovery environment that looks a lot like 1TR but isn't.
You won't be able to change anything from here- don't panic!
The only way we can change the startup security policy is in 1TR mode, which can only be entered by shutting the Mac down and then restarting while physically holding the power button.

Just shut the Mac down and then start it up while holding the power button, and try again.

### Troubleshooting
- If your Mac hangs / crashes when trying to boot the patched kernel, try setting the `-unsafe_kernel_text` boot argument (`sudo nvram boot-args="-unsafe_kernel_text"`).
   - You'll need to set this from a booted macOS, it won't work in recovery mode.
   - If your Mac is boot looping with the patched kernel, first uninstall it (see "reverting to normal" above), then try this.
- You may need to enable boot args with `bputil -a` in 1TR.
- `csrutil disable` in 1TR turns off SIP which can be useful here too.
- Adding a `-v` boot arg will give you a verbose boot log which can be helpful for debugging.
- Any kernel extensions you have installed will be compiled into the newly built kernelcache. So, if you have any kexts you will be updating soon (eg. if you are running [PacmanKit](https://github.com/jprx/PacmanKit)) make sure to leave those kexts out of the kernelcache build invocation.

### Disclaimer

These patches might cause system instability, security issues, or crashes. Additionally, they intentionally make your system less secure by enabling userspace access to the high resolution timers. Proceed at your own risk!

# What we are patching
See [patches](PATCHES.md) for explanations of what these patches are doing at the binary level.
Here is a high-level description of what we're doing.

Access to the performance counters is allowed when:

 - `PMCR0_EL1` has bit 30 (`PMCR0_USEREN_EN`) set to `1`.

Cache maintenance instructions (XNU calls these "DC MVA ops") are allowed when:

 - `SCTLR.UCI` (bit 26) is `1` (XNU default).
 - `SCTLR.DZE` (bit 14) is `1` (XNU default).
 - On P-Cores pre-H16, `HID4` has bit 11 set to `0` (mask with `~0x800`).
 - On E-Cores pre-H16, `EHID4` has bit 11 set to `0` (mask with `~0x800`).
 - On H16 cores, `ACFG_EL1` has bit 3 set to `0` (mask with `~0x08`).

### Register Map
| Register Name |     Encoding     |          Use          |
|---------------|------------------|-----------------------|
| `EHID4`       | `S3_0_C15_C4_1`  | DC MVA Ops on E-Cores |
| `HID4`        | `S3_0_C15_C4_0`  | DC MVA Ops on P-Cores |
| `ACFG_EL1`    | `S3_4_C15_C12_0` | DC MVA Ops for H16    |
| `PMCR0`       | `S3_1_C15_C0_0`  | Controls PMC Regs     |
| `PMC0`        | `S3_2_C15_C0_0`  | Cycle Counter         |
| `PMC1`        | `S3_2_C15_C1_0`  | Instruction Counter   |

References for PMCs:
 - "PMC[0-1] are the 48/64-bit fixed counters -- `S3_2_C15_C0_0` is cycles and `S3_2_C15_C1_0` is instructions" from: [monotonic_arm64.c](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/osfmk/arm64/monotonic_arm64.c#L77)
 - [`enable_counter` in `kpc.c`](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/osfmk/arm64/kpc.c#L267)
 - [`PMCR0_USEREN_ENABLE_MASK`](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/osfmk/arm64/kpc.c#L100)


References for *HID/ ACFG:
 - Reverse engineering kernel support libraries (see [DC_OPS.md](DC_OPS.md))
 - [`ENABLE_DC_MVA_OPS` macro in XNU](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/osfmk/arm64/caches_asm.s#L181)

# Documentation
- See [PATCHES](PATCHES.md) for an explanation of how we modify XNU to get the values we want into these registers.
- See [DC_OPS](DC_OPS.md) for an explanation of how the cache maintenance operations work on different SoCs.

