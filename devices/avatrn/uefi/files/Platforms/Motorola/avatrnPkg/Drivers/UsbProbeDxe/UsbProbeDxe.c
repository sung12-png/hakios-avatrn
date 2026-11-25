// Minimal USB bring-up probe: starts the vendor USB MSD device (UsbfnDwc3) with no
// storage attached, then services USB events from a periodic timer.
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/EFIUsbMsd.h>

STATIC EFI_USB_MSD_PROTOCOL *mMsd;

STATIC
VOID
EFIAPI
UsbTick (
  IN EFI_EVENT Event,
  IN VOID     *Context
  )
{
  mMsd->EventHandler (mMsd);
}

EFI_STATUS
EFIAPI
UsbProbeEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_EVENT  Ev;

  Status = gBS->LocateProtocol (&gEfiUsbMsdProtocolGuid, NULL, (VOID **)&mMsd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UsbProbe: no MSD protocol: %r\n", Status));
    return Status;
  }

  gBS->SetWatchdogTimer (0, 0, 0, NULL);

  Status = mMsd->StartDevice (mMsd);
  DEBUG ((DEBUG_ERROR, "UsbProbe: StartDevice -> %r\n", Status));
  if (EFI_ERROR (Status)) return Status;

  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, UsbTick, NULL, &Ev);
  if (!EFI_ERROR (Status)) {
    Status = gBS->SetTimer (Ev, TimerPeriodic, 100000);   // 10 ms
  }
  DEBUG ((DEBUG_ERROR, "UsbProbe: timer -> %r\n", Status));
  return EFI_SUCCESS;
}
