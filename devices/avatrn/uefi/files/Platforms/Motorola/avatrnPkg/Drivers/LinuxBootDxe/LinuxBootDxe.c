// Starts an arm64 Linux EFI-stub kernel that ABL loaded together with the UEFI FD.
// Payload layout (see pack_linux.py), at LNX_BASE:
//   +0x0000  u32 magic 'LNXB', u32 image_size, u32 dtb_size, u32 flags
//   +0x1000  Image (EFI stub PE/COFF)
//   next 4K-aligned: DTB
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Guid/Fdt.h>
#include <Guid/EventGroup.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/EFIUsbMsd.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionInfo.h>
#include <Protocol/LoadFile2.h>
#include <Uefi/UefiGpt.h>

#define LNX_BASE   0xA8600000ULL
#define LNX_MAGIC  0x42584E4C   // 'LNXB'
#define WAIT_SECS  10    // keep USB MSD gadget visible this long
#define MSD_ONLY 0   // 1: expose UFS as a USB disk forever, never boot. For flashing Windows.
#define STOP_USB_BEFORE_BOOT 0  // 1: StopDevice before starting Linux (ABL does this)
#define LIST_PARTS 0            // 1: print every GPT partition UEFI can see (read-only), then wait LIST_SECS
#define LIST_SECS  60
#define INITRD_PART  L"modem_b"  // partition holding: 4 KiB header {u32 "AVRD", u32 0, u64 size} + cpio archive (see mkavrd.py)
#define SKIP_EBS_EVENTS 0       // 1: do NOT run the firmware ExitBootServices handlers (they shut USB/PHY down)
#pragma clang diagnostic ignored "-Wunused-function"

STATIC VOID  *mMapBuf;
STATIC UINTN  mMapBufSize = 0x40000;

// Replacement for gBS->ExitBootServices: validates the map key like the real one, but does not
// signal the EXIT_BOOT_SERVICES event group, so Qualcomm/Motorola drivers do not tear down USB,
// PHY, clocks and PMIC state before Linux takes over.
STATIC
EFI_STATUS
EFIAPI
SkipEventsExitBootServices (
  IN EFI_HANDLE ImageHandle,
  IN UINTN MapKey
  )
{
  UINTN Size = mMapBufSize, Key, DescSize;
  UINT32 DescVer;
  EFI_STATUS S = gBS->GetMemoryMap (&Size, (EFI_MEMORY_DESCRIPTOR *)mMapBuf, &Key, &DescSize, &DescVer);
  if (EFI_ERROR (S)) return S;
  if (Key != MapKey) return EFI_INVALID_PARAMETER;
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  return EFI_SUCCESS;
}

STATIC VOID DumpUsbRegs (VOID)
{
  UINTN l, k;
  for (l = 0; l < 8; l++) {
    DEBUG ((DEBUG_ERROR, "G1490%02x: %08x %08x %08x %08x %08x %08x %08x %08x\n", l * 0x20,
      MmioRead32 (0x149000 + l*0x20), MmioRead32 (0x149004 + l*0x20), MmioRead32 (0x149008 + l*0x20), MmioRead32 (0x14900c + l*0x20),
      MmioRead32 (0x149010 + l*0x20), MmioRead32 (0x149014 + l*0x20), MmioRead32 (0x149018 + l*0x20), MmioRead32 (0x14901c + l*0x20)));
  }
  for (l = 0; l < 3; l++) {
    UINTN b = 0xa6f8800 + l * 0x20;
    DEBUG ((DEBUG_ERROR, "Q%03x: %08x %08x %08x %08x %08x %08x %08x %08x\n", 0x800 + l*0x20,
      MmioRead32 (b), MmioRead32 (b+4), MmioRead32 (b+8), MmioRead32 (b+12), MmioRead32 (b+16), MmioRead32 (b+20), MmioRead32 (b+24), MmioRead32 (b+28)));
  }
  for (l = 0; l < 9; l++) {
    UINTN b = 0x88e3000 + l * 0x20;
    DEBUG ((DEBUG_ERROR, "P%03x: %02x %02x %02x %02x %02x %02x %02x %02x\n", l*0x20,
      MmioRead32 (b) & 0xff, MmioRead32 (b+4) & 0xff, MmioRead32 (b+8) & 0xff, MmioRead32 (b+12) & 0xff, MmioRead32 (b+16) & 0xff, MmioRead32 (b+20) & 0xff, MmioRead32 (b+24) & 0xff, MmioRead32 (b+28) & 0xff));
  }
  for (k = 0; k < 1; k++) DEBUG ((DEBUG_ERROR, "DCTL %08x DSTS %08x GCTL %08x U2PHY %08x\n", MmioRead32 (0xa60c704), MmioRead32 (0xa60c70c), MmioRead32 (0xa60c110), MmioRead32 (0xa60c200)));
}

typedef struct { UINT32 Magic, ImageSize, DtbSize, Flags; } LNX_HDR;


typedef struct { CONST CHAR8 *Name; EFI_GUID Guid; } DIAG_DRV;
STATIC CONST DIAG_DRV mDiag[] = {
  { "UsbfnDwc3",  { 0x94F8A6A7, 0xDC34, 0x4101, { 0x88, 0xC1, 0x99, 0x17, 0x9C, 0xCE, 0xAE, 0x83 } } },
  { "UsbConfig",  { 0xCD823A4D, 0x7DEC, 0x4531, { 0xAE, 0x5D, 0x41, 0x34, 0xFA, 0x41, 0x27, 0xB8 } } },
  { "UsbInit",    { 0x0A134F0E, 0x075E, 0x40B3, { 0x9C, 0x63, 0x3B, 0x39, 0x06, 0x80, 0x46, 0x63 } } },
  { "UsbDevice",  { 0x3299A266, 0x15F0, 0x4346, { 0x83, 0x18, 0x71, 0x63, 0x36, 0x73, 0x6D, 0x3E } } },
  { "UsbMsd",     { 0x5AF77F10, 0x90DF, 0x4E7E, { 0x83, 0x25, 0xA1, 0x7E, 0xC0, 0x9D, 0x54, 0x43 } } },
  { "UsbPwrCtrl", { 0x11FAED4C, 0xB21F, 0x4D88, { 0x8E, 0x48, 0xC4, 0xC2, 0x8A, 0x1E, 0x50, 0xDF } } },
};

// Which of the USB drivers are really loaded, and how many handles carry the controller-init / driver-binding protocols
STATIC VOID Diag (CONST CHAR8 *Tag)
{
  EFI_HANDLE *H = NULL;
  UINTN Count = 0, n, d, found[sizeof (mDiag) / sizeof (mDiag[0])] = { 0 };
  if (!EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &Count, &H))) {
    for (n = 0; n < Count; n++) {
      EFI_LOADED_IMAGE_PROTOCOL *Li = NULL;
      if (EFI_ERROR (gBS->HandleProtocol (H[n], &gEfiLoadedImageProtocolGuid, (VOID **)&Li)) || Li->FilePath == NULL) continue;
      EFI_DEVICE_PATH_PROTOCOL *Dp = Li->FilePath;
      while (Dp->Type != 0x7F) {
        if (Dp->Type == 4 && Dp->SubType == 6) {   // MEDIA_PIWG_FW_FILE_DP
          EFI_GUID *G = (EFI_GUID *)(Dp + 1);
          for (d = 0; d < sizeof (mDiag) / sizeof (mDiag[0]); d++) if (CompareGuid (G, &mDiag[d].Guid)) found[d]++;
        }
        UINTN Len = Dp->Length[0] | ((UINTN)Dp->Length[1] << 8);
        if (Len < 4) break;
        Dp = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)Dp + Len);
      }
    }
    FreePool (H);
  }
  DEBUG ((DEBUG_ERROR, "Diag[%a] loaded: fn=%d cfg=%d init=%d dev=%d msd=%d pwr=%d\n", Tag,
          found[0], found[1], found[2], found[3], found[4], found[5]));
  H = NULL; Count = 0;
  gBS->LocateHandleBuffer (ByProtocol, &gUsbControllerInitGuid, NULL, &Count, &H);
  DEBUG ((DEBUG_ERROR, "Diag[%a] handles with UsbControllerInit protocol: %d\n", Tag, Count));
  if (H) FreePool (H);
  H = NULL; Count = 0;
  gBS->LocateHandleBuffer (ByProtocol, &gEfiDriverBindingProtocolGuid, NULL, &Count, &H);
  DEBUG ((DEBUG_ERROR, "Diag[%a] driver-binding handles: %d\n", Tag, Count));
  if (H) FreePool (H);
}

STATIC VOID EFIAPI DummyNotify (IN EFI_EVENT Event, IN VOID *Context) {}

// Same thing the platform boot manager does in DeviceBootManagerOnDemandConInConnect: signalling this
// event group makes UsbConfigDxe start the USB controller (PHY, clocks, Type-C) so that UsbfnDwc3Dxe can bind.
STATIC VOID StartUsbController (VOID)
{
  EFI_EVENT Ev;
  EFI_STATUS S = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, DummyNotify, NULL, &gUsbControllerInitGuid, &Ev);
  DEBUG ((DEBUG_ERROR, "LinuxBoot: create USB init event -> %r\n", S));
  if (!EFI_ERROR (S)) {
    gBS->SignalEvent (Ev);
    gBS->CloseEvent (Ev);
  }
  gBS->Stall (500000);
}

STATIC EFI_HANDLE mImageHandle;

// Read-only: scan every BlockIo handle, and for each physical (non-partition) device parse its GPT directly,
// so this works even when no partition driver publishes PartitionInfo. 3 entries per line.
STATIC VOID ListPartitions (VOID)
{
  EFI_HANDLE *H = NULL;
  UINTN Count = 0, n, total = 0;
  EFI_STATUS S;

  S = gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &H);
  DEBUG ((DEBUG_ERROR, "Parts: BlockIo handles: %d (%r), logical ones skipped below\n", S == EFI_SUCCESS ? Count : 0, S));
  if (EFI_ERROR (S)) return;
  for (n = 0; n < Count; n++) {
    EFI_BLOCK_IO_PROTOCOL *Bio = NULL;
    EFI_PARTITION_TABLE_HEADER *Hdr;
    UINT8 *Buf, *Ent;
    UINTN Bs, Need, i, col = 0, lines = 0;
    CHAR16 Line[3][52], Name[37];
    if (EFI_ERROR (gBS->HandleProtocol (H[n], &gEfiBlockIoProtocolGuid, (VOID **)&Bio)) || Bio->Media == NULL) continue;
    if (Bio->Media->LogicalPartition || !Bio->Media->MediaPresent) continue;
    DEBUG ((DEBUG_ERROR, "BIO %d: physical bs=%d last=%lx\n", n, Bio->Media->BlockSize, (UINT64)Bio->Media->LastBlock));
    Bs = Bio->Media->BlockSize;
    if (Bs < 512 || Bs > 0x10000) continue;
    Buf = AllocatePool (Bs);
    if (Buf == NULL) continue;
    if (EFI_ERROR (Bio->ReadBlocks (Bio, Bio->Media->MediaId, 1, Bs, Buf))) { DEBUG ((DEBUG_ERROR, "BIO %d: read LBA1 failed\n", n)); FreePool (Buf); continue; }
    Hdr = (EFI_PARTITION_TABLE_HEADER *)Buf;
    if (Hdr->Header.Signature != EFI_PTAB_HEADER_ID) { DEBUG ((DEBUG_ERROR, "BIO %d: no GPT\n", n)); FreePool (Buf); continue; }
    Need = ((UINTN)Hdr->NumberOfPartitionEntries * Hdr->SizeOfPartitionEntry + Bs - 1) / Bs * Bs;
    if (Need == 0 || Need > 0x40000) { FreePool (Buf); continue; }
    Ent = AllocatePool (Need);
    if (Ent == NULL || EFI_ERROR (Bio->ReadBlocks (Bio, Bio->Media->MediaId, Hdr->PartitionEntryLBA, Need, Ent))) {
      DEBUG ((DEBUG_ERROR, "BIO %d: read entries failed\n", n)); if (Ent) FreePool (Ent); FreePool (Buf); continue;
    }
    DEBUG ((DEBUG_ERROR, "BIO %d: GPT, %d entries; name SizeMB/StartLBA(hex)\n", n, Hdr->NumberOfPartitionEntries));
    for (i = 0; i < Hdr->NumberOfPartitionEntries; i++) {
      EFI_PARTITION_ENTRY *E = (EFI_PARTITION_ENTRY *)(Ent + i * Hdr->SizeOfPartitionEntry);
      UINT64 Mb;
      if (IsZeroBuffer (&E->PartitionTypeGUID, sizeof (EFI_GUID))) continue;
      CopyMem (Name, E->PartitionName, 36 * sizeof (CHAR16));
      Name[36] = 0;
      Mb = DivU64x32 (MultU64x32 (E->EndingLBA - E->StartingLBA + 1, (UINT32)Bs), 1048576);
      UnicodeSPrint (Line[col], sizeof (Line[col]), L"%s %lu/%lx", Name, Mb, E->StartingLBA);
      total++;
      if (++col == 2) {
        DEBUG ((DEBUG_ERROR, "%s | %s\n", Line[0], Line[1]));
        col = 0;
        if (++lines % 20 == 0) { DEBUG ((DEBUG_ERROR, "---- page, photograph now ----\n")); gBS->Stall (15000000); }
      }
    }
    if (col == 1) DEBUG ((DEBUG_ERROR, "%s\n", Line[0]));
    FreePool (Ent);
    FreePool (Buf);
  }
  DEBUG ((DEBUG_ERROR, "Parts: %d GPT entries listed\n", total));
  FreePool (H);
}

// ---- initrd from a UFS partition, handed to the kernel EFI stub through the LINUX_EFI_INITRD_MEDIA_GUID LoadFile2 protocol ----
#define AVRD_MAGIC 0x44525641   // 'AVRD'
#define LINUX_EFI_INITRD_MEDIA_GUID_INIT { 0x5568e427, 0x68fc, 0x4f3d, { 0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68 } }
#pragma pack(1)
typedef struct { VENDOR_DEVICE_PATH Vendor; EFI_DEVICE_PATH_PROTOCOL End; } INITRD_DP;
#pragma pack()
STATIC INITRD_DP mInitrdDp = {
  { { MEDIA_DEVICE_PATH, MEDIA_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } }, LINUX_EFI_INITRD_MEDIA_GUID_INIT },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};
STATIC VOID  *mInitrd;
STATIC UINTN  mInitrdSize;

STATIC
EFI_STATUS
EFIAPI
InitrdLoadFile2 (
  IN EFI_LOAD_FILE2_PROTOCOL *This,
  IN EFI_DEVICE_PATH_PROTOCOL *FilePath,
  IN BOOLEAN BootPolicy,
  IN OUT UINTN *BufferSize,
  IN VOID *Buffer OPTIONAL
  )
{
  if (BootPolicy) return EFI_UNSUPPORTED;
  if (BufferSize == NULL) return EFI_INVALID_PARAMETER;
  if (Buffer == NULL || *BufferSize < mInitrdSize) { *BufferSize = mInitrdSize; return EFI_BUFFER_TOO_SMALL; }
  CopyMem (Buffer, mInitrd, mInitrdSize);
  *BufferSize = mInitrdSize;
  return EFI_SUCCESS;
}
STATIC EFI_LOAD_FILE2_PROTOCOL mInitrdLf2 = { InitrdLoadFile2 };

// Read-only on the device: find the GPT partition by name, validate the AVRD header, read the archive into RAM.
STATIC VOID LoadInitrdFromPartition (VOID)
{
  EFI_HANDLE *H = NULL, Hn = NULL;
  UINTN Count = 0, n;
  if (EFI_ERROR (gBS->LocateHandleBuffer (ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &Count, &H))) return;
  for (n = 0; n < Count; n++) {
    EFI_BLOCK_IO_PROTOCOL *Bio = NULL;
    EFI_PARTITION_INFO_PROTOCOL *Pi = NULL;
    CHAR16 Name[37];
    UINT8 *Hdr;
    UINT64 Size, Off, PartBytes;
    UINTN Bs, Chunk;
    EFI_STATUS S;
    if (EFI_ERROR (gBS->HandleProtocol (H[n], &gEfiBlockIoProtocolGuid, (VOID **)&Bio)) || Bio->Media == NULL) continue;
    if (!Bio->Media->LogicalPartition || !Bio->Media->MediaPresent) continue;
    if (EFI_ERROR (gBS->HandleProtocol (H[n], &gEfiPartitionInfoProtocolGuid, (VOID **)&Pi)) || Pi->Type != PARTITION_TYPE_GPT) continue;
    CopyMem (Name, Pi->Info.Gpt.PartitionName, 36 * sizeof (CHAR16));
    Name[36] = 0;
    if (StrCmp (Name, INITRD_PART) != 0) continue;
    Bs = Bio->Media->BlockSize;
    PartBytes = MultU64x32 (Bio->Media->LastBlock + 1, (UINT32)Bs);
    Hdr = AllocatePool (Bs);
    if (Hdr == NULL) break;
    S = Bio->ReadBlocks (Bio, Bio->Media->MediaId, 0, Bs, Hdr);
    if (EFI_ERROR (S)) { DEBUG ((DEBUG_ERROR, "Initrd: header read -> %r\n", S)); FreePool (Hdr); break; }
    if (*(UINT32 *)Hdr != AVRD_MAGIC) { DEBUG ((DEBUG_ERROR, "Initrd: no AVRD header in %s, skipping\n", INITRD_PART)); FreePool (Hdr); break; }
    Size = *(UINT64 *)(Hdr + 8);
    FreePool (Hdr);
    if (Size == 0 || Size + Bs > PartBytes) { DEBUG ((DEBUG_ERROR, "Initrd: bad size %lx (partition %lx)\n", Size, PartBytes)); break; }
    mInitrd = AllocatePages (EFI_SIZE_TO_PAGES ((UINTN)Size));
    if (mInitrd == NULL) { DEBUG ((DEBUG_ERROR, "Initrd: out of memory for %lx bytes\n", Size)); break; }
    Chunk = 0x100000;   // 1 MiB reads
    for (Off = 0; Off < Size; Off += Chunk) {
      UINTN Len = (UINTN)((Size - Off) < Chunk ? (Size - Off) : Chunk);
      UINTN Rd = (Len + Bs - 1) / Bs * Bs;
      if (Off + Rd > EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES ((UINTN)Size))) Rd = EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES ((UINTN)Size)) - (UINTN)Off;
      S = Bio->ReadBlocks (Bio, Bio->Media->MediaId, 1 + DivU64x32 (Off, (UINT32)Bs), Rd, (UINT8 *)mInitrd + Off);
      if (EFI_ERROR (S)) { DEBUG ((DEBUG_ERROR, "Initrd: read at %lx -> %r\n", Off, S)); mInitrd = NULL; break; }
    }
    if (mInitrd == NULL) break;
    mInitrdSize = (UINTN)Size;
    S = gBS->InstallMultipleProtocolInterfaces (&Hn, &gEfiDevicePathProtocolGuid, &mInitrdDp, &gEfiLoadFile2ProtocolGuid, &mInitrdLf2, NULL);
    DEBUG ((DEBUG_ERROR, "Initrd: %s -> %lx bytes loaded, LoadFile2 install -> %r\n", INITRD_PART, Size, S));
    break;
  }
  FreePool (H);
}

STATIC
VOID
ConnectAllControllers (VOID)
{
  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0, n;
  if (!EFI_ERROR (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles))) {
    for (n = 0; n < Count; n++) gBS->ConnectController (Handles[n], NULL, NULL, TRUE);
    FreePool (Handles);
  }
}

STATIC
VOID
EFIAPI
BootLinux (
  IN EFI_EVENT Event,
  IN VOID     *Context
  )
{
  EFI_HANDLE ImageHandle = mImageHandle;
  LNX_HDR *H = (LNX_HDR *)(UINTN)LNX_BASE;
  EFI_STATUS Status;
  EFI_HANDLE Kernel = NULL;
  UINTN Pages;
  VOID *Dtb;
  EFI_USB_MSD_PROTOCOL *Msd = NULL;
  UINTN i;
  gBS->RestoreTPL (TPL_APPLICATION);
  #if MSD_ONLY
  {
    EFI_USB_MSD_PROTOCOL *M = NULL;
    EFI_STATUS S;
    StartUsbController ();
    ConnectAllControllers ();
    gBS->SetWatchdogTimer (0, 0, 0, NULL);
    S = gBS->LocateProtocol (&gEfiUsbMsdProtocolGuid, NULL, (VOID **)&M);
    DEBUG ((DEBUG_ERROR, "MSD_ONLY: Msd protocol -> %r\n", S));
    if (!EFI_ERROR (S)) {
      S = M->StartDevice (M);
      DEBUG ((DEBUG_ERROR, "MSD_ONLY: StartDevice -> %r; UFS is now a USB disk\n", S));
      for (;;) { gBS->Stall (10000); M->EventHandler (M); }   // service forever
    }
    DEBUG ((DEBUG_ERROR, "MSD_ONLY: no MSD protocol, halting\n"));
    for (;;) gBS->Stall (1000000);
  }
  #endif

  if (H->Magic != LNX_MAGIC) {
    DEBUG ((DEBUG_ERROR, "LinuxBoot: no payload at %lx (magic %x)\n", (UINT64)LNX_BASE, H->Magic));
    gBS->Stall (10000000);   // keep the message on screen
    return;
  }
  UINT8 *Img = (UINT8 *)(UINTN)(LNX_BASE + 0x1000);
  UINT8 *DtbSrc = (UINT8 *)(UINTN)(LNX_BASE + 0x1000 + ((H->ImageSize + 0xFFF) & ~0xFFFULL));
  DEBUG ((DEBUG_ERROR, "LinuxBoot: image %x bytes, dtb %x bytes, starting in %d s\n", H->ImageSize, H->DtbSize, WAIT_SECS));

  // All DXE drivers are dispatched now (EndOfDxe). The USB function driver binds to the USB config
  // handle only when controllers are connected, so do what BDS would do before using it.
  Diag ("pre");
  StartUsbController ();
  ConnectAllControllers ();
  Diag ("post");
#if LIST_PARTS
  ListPartitions ();
  gBS->Stall (LIST_SECS * 1000000);
#endif
  gBS->Stall (6000000);
  {
    VOID *Fn = NULL;
    DEBUG ((DEBUG_ERROR, "LinuxBoot: UsbfnIo -> %r\n", gBS->LocateProtocol (&gEfiUsbfnIoProtocolGuid, NULL, &Fn)));
  }

  // Start the vendor USB gadget ourselves (this initialises PHY, clocks and the Type-C path),
  // keep it serviced for WAIT_SECS so the host enumerates it, then leave it running for Linux.
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  Status = gBS->LocateProtocol (&gEfiUsbMsdProtocolGuid, NULL, (VOID **)&Msd);
  DEBUG ((DEBUG_ERROR, "LinuxBoot: Msd protocol -> %r\n", Status));
  if (!EFI_ERROR (Status)) {
    Status = Msd->StartDevice (Msd);
    DEBUG ((DEBUG_ERROR, "LinuxBoot: StartDevice -> %r\n", Status));
    for (i = 0; i < WAIT_SECS * 100; i++) {
      gBS->Stall (10000);
      Msd->EventHandler (Msd);
    }
#if STOP_USB_BEFORE_BOOT
    Status = Msd->StopDevice (Msd);
    DEBUG ((DEBUG_ERROR, "LinuxBoot: StopDevice -> %r\n", Status));
#endif
  } else {
    for (i = 0; i < WAIT_SECS; i++) gBS->Stall (1000000);
  }
  DumpUsbRegs ();
  gBS->Stall (8000000);   // time to photograph the dump

  LoadInitrdFromPartition ();

  // Device tree into EFI memory and the configuration table
  Pages = EFI_SIZE_TO_PAGES (H->DtbSize + 0x1000);
  Dtb = AllocatePages (Pages);
  if (Dtb == NULL) return;
  CopyMem (Dtb, DtbSrc, H->DtbSize);
  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, Dtb);
  DEBUG ((DEBUG_ERROR, "LinuxBoot: FDT table -> %r\n", Status));

  Status = gBS->LoadImage (FALSE, ImageHandle, NULL, Img, H->ImageSize, &Kernel);
  DEBUG ((DEBUG_ERROR, "LinuxBoot: LoadImage -> %r\n", Status));
  if (EFI_ERROR (Status)) { gBS->Stall (10000000); return; }

#if SKIP_EBS_EVENTS
  mMapBuf = AllocatePool (mMapBufSize);
  if (mMapBuf != NULL) {
    gBS->ExitBootServices = SkipEventsExitBootServices;
    gBS->Hdr.CRC32 = 0;
    gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);
    DEBUG ((DEBUG_ERROR, "LinuxBoot: ExitBootServices events skipped\n"));
  }
#endif
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  Status = gBS->StartImage (Kernel, NULL, NULL);
  DEBUG ((DEBUG_ERROR, "LinuxBoot: StartImage returned %r\n", Status));
  gBS->Stall (10000000);
}

EFI_STATUS
EFIAPI
LinuxBootEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_EVENT Ev;
  mImageHandle = ImageHandle;
  return gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, BootLinux, NULL, &gEfiEndOfDxeEventGroupGuid, &Ev);
}

