# avatrn (Motorola Edge 2024, XT2405, SM7435): UEFI

    devices/avatrn/uefi/apply.sh
    cd Mu-Silicium && python3 build_uefi.py -d avatrn -r RELEASE

## Not here: supply these yourself, or the build fails
- `Binaries/avatrn/`: the vendor's UEFI drivers and raw files, from your own phone's xbl (UEFIReader, as
  Mu-Silicium's porting guide describes). `Platforms/Motorola/avatrnPkg/Include/*.inc` names them.
- `Resources/DTBs/avatrn.dtb`: your phone's device tree.
- ACPI tables for `INF RuleOverride = ACPITABLE avatrn/AcpiTables.inf` (a platform in Silicium-ACPI).

## Status
Taken from the working tree that built the firmware iOS was booted with. A build from a fresh checkout
with this folder has **not** been tried. The XNU payload is at 0xF4000000: build the loader with
`PAYLOAD_BASE=0xF4000000ULL` (see ../../../avatrn/README.md).
