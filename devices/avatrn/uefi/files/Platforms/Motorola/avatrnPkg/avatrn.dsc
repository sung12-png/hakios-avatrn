##
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = avatrn
  PLATFORM_GUID                  = 55A235FD-F0FD-4502-AC49-DCE02FD19192
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/avatrnPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = avatrnPkg/avatrn.fdf
  USE_CUSTOM_DISPLAY_DRIVER      = 0

  #
  # 0 = SM7435
  # 1 = SM6450
  #
  SOC_TYPE                       = 0

!include NetraniPkg/NetraniPkg.dsc.inc

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0xA760D000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"Motorola"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"Moto Edge 2024"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"avatrn"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"Edge_2024_avatrn"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"Moto Edge 2024"

  #
  # Simple Frame Buffer
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|1080
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|2400
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Platform PEI
  #
  gQcomPkgTokenSpaceGuid.PcdPlatformType|"LA"

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|avatrnPkg/Library/MemoryMapLib/MemoryMapLib.inf
  DxeMemoryProtectionHobLib|MdeModulePkg/Library/MemoryProtectionHobLibNull/DxeMemoryProtectionHobLibNull.inf

  #
  # QCOM Libraries
  #
  ConfigurationMapLib|avatrnPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[LibraryClasses.common.DXE_CORE]
  DxeMemoryProtectionHobLib|MdeModulePkg/Library/MemoryProtectionHobLibNull/DxeMemoryProtectionHobLibNull.inf

[Components]
  #
  avatrnPkg/Drivers/SpmiAbortSkipDxe/SpmiAbortSkipDxe.inf
  # Off: this driver installs a 50 ms periodic timer at TPL_CALLBACK that calls
  # the vendor ButtonsDxe provider, which can stall BDS before the splash.
  # The 13:17 reference image does not contain it. Re-enable to test buttons.
  # avatrnPkg/Drivers/ButtonKeyDxe/ButtonKeyDxe.inf
  avatrnPkg/Drivers/UsbProbeDxe/UsbProbeDxe.inf
  # avatrnPkg/Drivers/LinuxBootDxe/LinuxBootDxe.inf

  # APRIORI bisection beacons - off: they paint full-width bands over the
  # framebuffer during dispatch, and the 13:17 reference build carries none.
  # avatrnPkg/Drivers/BeaconDxe/Beacon3.inf
  # avatrnPkg/Drivers/BeaconDxe/Beacon4.inf
  # avatrnPkg/Drivers/BeaconDxe/Beacon5.inf
  # avatrnPkg/Drivers/BeaconDxe/Beacon6.inf
  # avatrnPkg/Drivers/BeaconDxe/Beacon7.inf
  # avatrnPkg/Drivers/BeaconDxe/Beacon8.inf

  # ACPI Tables
  #
  avatrn/AcpiTables.inf
