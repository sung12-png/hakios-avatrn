// Tiny DXE driver that paints one colored band on the framebuffer at entry.
// Used to bisect APRIORI dispatch: insert this at a checkpoint, boot, see which
// bands appeared before the hang. One source, several INFs; BEACON_SLOT /
// BEACON_COLOR / BEACON_ENTRY come from each INF's [BuildOptions].
#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>

#ifndef BEACON_SLOT
#define BEACON_SLOT 0
#endif
#ifndef BEACON_COLOR
#define BEACON_COLOR 0x00FFFFFF
#endif
#ifndef BEACON_ENTRY
#define BEACON_ENTRY BeaconEntry
#endif

#define FB_BASE    0xB8000000ULL
#define FB_WIDTH   1080
#define FB_HEIGHT  2400
#define FB_STRIDE  (FB_WIDTH * 4)
#define FB_BAND_H  12

EFI_STATUS
EFIAPI
BEACON_ENTRY (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  volatile UINT32  *P;
  UINT32            x;
  UINT32            y;
  UINT32            y0 = (UINT32)(BEACON_SLOT) * (FB_BAND_H + 4);

  if (y0 + FB_BAND_H < FB_HEIGHT) {
    for (y = 0; y < FB_BAND_H; y++) {
      P = (volatile UINT32 *)(UINTN)(FB_BASE + (y0 + y) * FB_STRIDE);
      for (x = 0; x < FB_WIDTH; x++) {
        P[x] = (UINT32)(BEACON_COLOR);
      }
    }
  }
  return EFI_SUCCESS;
}
