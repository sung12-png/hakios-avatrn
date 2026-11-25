APRIORI DXE {
  INF MdeModulePkg/Core/Dxe/DxeMain.inf
  INF MdeModulePkg/Universal/PCD/Dxe/Pcd.inf

  INF Binaries/avatrn/QcomPkg/Drivers/EnvDxe/EnvDxe.inf

  INF MdeModulePkg/Universal/ReportStatusCodeRouter/RuntimeDxe/ReportStatusCodeRouterRuntimeDxe.inf
  INF MdeModulePkg/Universal/StatusCodeHandler/RuntimeDxe/StatusCodeHandlerRuntimeDxe.inf

  INF Binaries/avatrn/MdeModulePkg/Universal/TimestampDxe/TimestampDxe.inf

  INF MdeModulePkg/Core/RuntimeDxe/RuntimeDxe.inf
  INF ArmPkg/Drivers/CpuDxe/CpuDxe.inf
  INF ArmPkg/Drivers/ArmGicDxe/ArmGicDxe.inf
  INF EmbeddedPkg/MetronomeDxe/MetronomeDxe.inf
  INF ArmPkg/Drivers/TimerDxe/TimerDxe.inf
  INF avatrnPkg/Drivers/SpmiAbortSkipDxe/SpmiAbortSkipDxe.inf
  # INF avatrnPkg/Drivers/BeaconDxe/Beacon3.inf  # orange: past SpmiAbortSkip

  INF Binaries/avatrn/MbmPkg/Core/Drivers/MotBootFlagsDxe/MotBootFlagsDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/SmemDxe/SmemDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/DALSYSDxe/DALSYSDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/HWIODxe/HWIODxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/ChipInfoDxe/ChipInfoDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/PlatformInfoDxe/PlatformInfoDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/HALIOMMUDxe/HALIOMMUDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/ULogDxe/ULogDxe.inf
  # INF avatrnPkg/Drivers/BeaconDxe/Beacon4.inf  # magenta: past Smem/DALSYS/HWIO/ChipInfo/PlatformInfo/HALIOMMU/ULog
  INF Binaries/avatrn/QcomPkg/Drivers/IPCCDxe/IPCCDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/GLinkDxe/GLinkDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/CmdDbDxe/CmdDbDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/PwrUtilsDxe/PwrUtilsDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/NpaDxe/NpaDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/RpmhDxe/RpmhDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/VcsDxe/VcsDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/ClockDxe/ClockDxe.inf
  # INF avatrnPkg/Drivers/BeaconDxe/Beacon5.inf  # blue: past IPCC/GLink/CmdDb/PwrUtils/Npa/Rpmh/Vcs/Clock
  INF Binaries/avatrn/QcomPkg/Drivers/ICBDxe/ICBDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/ShmBridgeDxe/ShmBridgeDxeLA.inf
  INF Binaries/avatrn/QcomPkg/Drivers/TzDxe/ScmDxeLA.inf
  INF Binaries/avatrn/QcomPkg/Drivers/TLMMDxe/TLMMDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/I2CDxe/I2CDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/SPMIDxe/SPMIDxe.inf
  # INF avatrnPkg/Drivers/BeaconDxe/Beacon6.inf  # purple: past ICB/ShmBridge/ScmDxeLA/TLMM/I2C/SPMI

  INF MdeModulePkg/Universal/ResetSystemRuntimeDxe/ResetSystemRuntimeDxe.inf

  INF Binaries/avatrn/QcomPkg/Drivers/PmicDxe/PmicDxeLa.inf
  # INF avatrnPkg/Drivers/BeaconDxe/Beacon7.inf  # teal: past PmicDxe

  INF MdeModulePkg/Universal/Disk/DiskIoDxe/DiskIoDxe.inf
  INF MdeModulePkg/Universal/Disk/PartitionDxe/PartitionDxe.inf
  INF MdeModulePkg/Universal/Disk/UnicodeCollation/EnglishDxe/EnglishDxe.inf

  INF Binaries/avatrn/QcomPkg/Drivers/SdccDxe/SdccDxe.inf

  INF Binaries/avatrn/QcomPkg/Drivers/UFSDxe/UFSDxe.inf
  # INF avatrnPkg/Drivers/BeaconDxe/Beacon8.inf  # white: past SdccDxe/UFSDxe

  INF FatPkg/EnhancedFatDxe/Fat.inf

  # INF Binaries/avatrn/QcomPkg/Drivers/TzDxe/TzDxeLA.inf
  # INF Binaries/avatrn/MbmPkg/Core/Drivers/ProvDxe/ProvDxe.inf

  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableRuntimeDxe.inf

  INF Binaries/avatrn/QcomPkg/Drivers/DDRInfoDxe/DDRInfoDxe.inf
  INF Binaries/avatrn/MbmPkg/Core/Drivers/MotStorageDxe/MotStorageDxe.inf
  INF Binaries/avatrn/MbmPkg/Core/Drivers/MotUtagsDxe/MotUtagsDxe.inf
  INF Binaries/avatrn/MbmPkg/Core/Drivers/MotUtagsDictDxe/MotUtagsDictDxe.inf
  INF Binaries/avatrn/MbmPkg/Core/Drivers/MotHwIdDxe/MotHwIdDxe.inf
  # INF Binaries/avatrn/MbmPkg/Core/Drivers/MotCoreServicesDxe/MotCoreServicesDxe.inf
  # INF Binaries/avatrn/MbmPkg/Core/Drivers/AbDxe/AbDxe.inf

!if $(USE_CUSTOM_DISPLAY_DRIVER) == 1
  INF Binaries/avatrn/QcomPkg/Drivers/DisplayDxe/DisplayDxe.inf
!else
  INF SiliciumPkg/Drivers/SimpleFbDxe/SimpleFbDxe.inf
!endif

  # INF Binaries/avatrn/QcomPkg/Drivers/PILProxyDxe/PILProxyDxe.inf
  # INF Binaries/avatrn/QcomPkg/Drivers/PILDxe/PILDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/SPSSDxe/SPSSDxe.inf

  INF MdeModulePkg/Universal/WatchdogTimerDxe/WatchdogTimer.inf
  INF MdeModulePkg/Universal/SecurityStubDxe/SecurityStubDxe.inf
  INF EmbeddedPkg/EmbeddedMonotonicCounter/EmbeddedMonotonicCounter.inf
  INF EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf
  INF MdeModulePkg/Universal/PrintDxe/PrintDxe.inf
  INF MdeModulePkg/Universal/DevicePathDxe/DevicePathDxe.inf
  INF MdeModulePkg/Universal/CapsuleRuntimeDxe/CapsuleRuntimeDxe.inf
  INF MdeModulePkg/Universal/HiiDatabaseDxe/HiiDatabaseDxe.inf
  INF MdeModulePkg/Universal/BdsDxe/BdsDxe.inf

  INF Binaries/avatrn/QcomPkg/Drivers/SPIDxe/SPIDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/PmicGlinkDxe/PmicGlinkDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/UsbPwrCtrlDxe/UsbPwrCtrlDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/QcomChargerDxe/QcomChargerDxeLA.inf
  INF Binaries/avatrn/QcomPkg/Drivers/ChargerExDxe/ChargerExDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/UsbfnDwc3Dxe/UsbfnDwc3Dxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/XhciPciEmulationDxe/XhciPciEmulationDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/XhciDxe/XhciDxe.inf

  INF MdeModulePkg/Bus/Usb/UsbBusDxe/UsbBusDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbKbDxe/UsbKbDxe.inf
  INF MdeModulePkg/Bus/Usb/UsbMassStorageDxe/UsbMassStorageDxe.inf

  INF Binaries/avatrn/QcomPkg/Drivers/UsbMsdDxe/UsbMsdDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/UsbDeviceDxe/UsbDeviceDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/UsbConfigDxe/UsbConfigDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/UsbInitDxe/UsbInitDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/ButtonsDxe/ButtonsDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/TsensDxe/TsensDxe.inf

!if $(USE_CUSTOM_DISPLAY_DRIVER) == 1
  INF Binaries/avatrn/QcomPkg/Drivers/CPRDxe/CPRDxe.inf
!endif

  INF Binaries/avatrn/QcomPkg/Drivers/GpiDxe/GpiDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/UCDxe/UCDxe.inf
  INF Binaries/avatrn/QcomPkg/Drivers/RNGDxe/RngDxe.inf

  INF EmbeddedPkg/SimpleTextInOutSerial/SimpleTextInOutSerial.inf
  INF MdeModulePkg/Universal/Console/ConPlatformDxe/ConPlatformDxe.inf
  INF MdeModulePkg/Universal/Console/ConSplitterDxe/ConSplitterDxe.inf
  INF MdeModulePkg/Universal/Console/GraphicsConsoleDxe/GraphicsConsoleDxe.inf

  # INF Binaries/avatrn/MbmPkg/Core/Drivers/MbmModeDxe/MbmModeDxe.inf
  INF Binaries/avatrn/MbmPkg/Core/Drivers/MotSolDxe/MotSolDxe.inf
  # INF Binaries/avatrn/MbmPkg/Core/Drivers/BacklightDxe/BacklightDxe.inf
}
