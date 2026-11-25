/** @file
  Button -> console key bridge for the firmware.

  Qualcomm's ButtonsDxe (prebuilt, packed for avatrn) publishes only
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL. Windows Boot Manager reads the Ex
  protocol, not gST->ConIn: wrapping ConIn alone only ever stopped the
  countdown, it never started an entry.

  So the Ex protocol is reinstalled on the provider handle with a wrapper.
  The console splitter re-reads the interface after the reinstall, which means
  both a direct Ex consumer and the aggregate console path go through us.

  Mapping:
    - volume up / down arrive as SCAN_UP / SCAN_DOWN and pass through so the
      menu highlight moves;
    - every other key (power, vendor codes) is reported as Enter 0x000D twice,
      because the boot manager eats the first keystroke to cancel its timeout;
    - the raw ScanCode / UnicodeChar of each press is printed so a run tells us
      what the hardware actually reports.

  Copyright (c) 2026. SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextInEx.h>

#define KEY_QUEUE_SIZE  8

STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL      *mOrigConIn = NULL;
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL       mOurConIn;
STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL   *mOrigExPtr = NULL;
STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL    mOrigEx;
STATIC EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL    mOurEx;
STATIC EFI_EVENT                            mWaitEvent = NULL;
STATIC EFI_EVENT                            mTimerEvent = NULL;
STATIC EFI_INPUT_KEY                        mQueue[KEY_QUEUE_SIZE];
STATIC UINTN                                mQueueHead = 0;
STATIC UINTN                                mQueueTail = 0;

//
// RELEASE builds map DebugLib to BaseDebugLibNull, so DEBUG() prints nothing
// there. These lines go straight to ConOut so the driver is visible either way.
//
STATIC EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL     *mOut       = NULL;
STATIC CHAR16                               mLine[64];
STATIC UINTN                                mLineLen   = 0;
STATIC UINTN                                mKeysShown = 0;

STATIC
VOID
LineReset (
  VOID
  )
{
  mLineLen  = 0;
  mLine[0]  = 0;
}

STATIC
VOID
LinePut (
  IN CONST CHAR16  *S
  )
{
  while ((*S != 0) && (mLineLen < 60)) {
    mLine[mLineLen++] = *S++;
  }

  mLine[mLineLen] = 0;
}

STATIC
VOID
LineHex (
  IN UINTN  V,
  IN UINTN  Digits
  )
{
  CONST CHAR16  *D = L"0123456789ABCDEF";
  CHAR16         One[2];
  UINTN          i;

  One[1] = 0;
  for (i = 0; i < Digits; i++) {
    One[0] = D[(V >> ((Digits - 1 - i) * 4)) & 0xF];
    LinePut (One);
  }
}

STATIC
VOID
LineFlush (
  VOID
  )
{
  LinePut (L"\r\n");
  if (mOut != NULL) {
    mOut->OutputString (mOut, mLine);
  }

  LineReset ();
}

STATIC
BOOLEAN
QueueEmpty (
  VOID
  )
{
  return (BOOLEAN)(mQueueHead == mQueueTail);
}

STATIC
BOOLEAN
QueueFull (
  VOID
  )
{
  return (BOOLEAN)(((mQueueTail + 1) % KEY_QUEUE_SIZE) == mQueueHead);
}

STATIC
VOID
QueuePush (
  IN EFI_INPUT_KEY  *Key
  )
{
  if (QueueFull ()) {
    return;
  }

  mQueue[mQueueTail] = *Key;
  mQueueTail         = (mQueueTail + 1) % KEY_QUEUE_SIZE;

  if (mWaitEvent != NULL) {
    gBS->SignalEvent (mWaitEvent);
  }
}

STATIC
BOOLEAN
QueuePop (
  OUT EFI_INPUT_KEY  *Key
  )
{
  if (QueueEmpty ()) {
    return FALSE;
  }

  *Key       = mQueue[mQueueHead];
  mQueueHead = (mQueueHead + 1) % KEY_QUEUE_SIZE;
  return TRUE;
}

//
// Read one raw key from the hardware and translate it into what the boot
// manager expects. Always read from mOrigEx, never from our own wrapper.
//
STATIC
BOOLEAN
PollRaw (
  OUT EFI_KEY_DATA  *Raw
  )
{
  EFI_STATUS  Status;
  EFI_KEY_DATA KeyData;

  if (mOrigEx.ReadKeyStrokeEx == NULL) {
    return FALSE;
  }

  ZeroMem (&KeyData, sizeof (KeyData));
  Status = mOrigEx.ReadKeyStrokeEx (&mOrigEx, &KeyData);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  *Raw = KeyData;
  return TRUE;
}

STATIC
VOID
TranslateAndQueue (
  IN EFI_KEY_DATA  *Raw
  )
{
  EFI_INPUT_KEY Enter;

  DEBUG ((
    DEBUG_ERROR,
    "[ButtonKey] sc=%04x uc=%04x\n",
    Raw->Key.ScanCode,
    Raw->Key.UnicodeChar
    ));

  if (mKeysShown < 8) {
    mKeysShown++;
    LinePut (L"BK: sc=");
    LineHex (Raw->Key.ScanCode, 4);
    LinePut (L" uc=");
    LineHex (Raw->Key.UnicodeChar, 4);
    LineFlush ();
  }

  if ((Raw->Key.ScanCode == SCAN_UP) || (Raw->Key.ScanCode == SCAN_DOWN)) {
    QueuePush (&Raw->Key);
    return;
  }

  if ((Raw->Key.ScanCode == SCAN_NULL) && (Raw->Key.UnicodeChar != CHAR_NULL)) {
    QueuePush (&Raw->Key);
    return;
  }

  Enter.ScanCode    = SCAN_NULL;
  Enter.UnicodeChar = 0x000D;
  QueuePush (&Enter);
  QueuePush (&Enter);
}

STATIC
VOID
PollButtons (
  VOID
  )
{
  EFI_KEY_DATA  Raw;

  while (PollRaw (&Raw)) {
    TranslateAndQueue (&Raw);
  }
}

STATIC
VOID
EFIAPI
ButtonKeyTimer (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  PollButtons ();
}

//
// EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL wrapper -- this is the path bootmgr uses.
//
STATIC
EFI_STATUS
EFIAPI
OurExReset (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN BOOLEAN                            ExtendedVerification
  )
{
  mQueueHead = 0;
  mQueueTail = 0;

  if ((mOrigEx.Reset != NULL) && (mOrigEx.Reset != OurExReset)) {
    return mOrigEx.Reset (&mOrigEx, ExtendedVerification);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
OurExReadKeyStrokeEx (
  IN  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  OUT EFI_KEY_DATA                       *KeyData
  )
{
  EFI_INPUT_KEY  Key;

  PollButtons ();

  if (QueuePop (&Key)) {
    ZeroMem (KeyData, sizeof (*KeyData));
    KeyData->Key = Key;
    return EFI_SUCCESS;
  }

  if ((mOrigEx.ReadKeyStrokeEx != NULL) && (mOrigEx.ReadKeyStrokeEx != OurExReadKeyStrokeEx)) {
    return mOrigEx.ReadKeyStrokeEx (&mOrigEx, KeyData);
  }

  return EFI_NOT_READY;
}

STATIC
EFI_STATUS
EFIAPI
OurExSetState (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN EFI_KEY_TOGGLE_STATE               *KeyToggleState
  )
{
  if ((mOrigEx.SetState != NULL) && (mOrigEx.SetState != OurExSetState)) {
    return mOrigEx.SetState (&mOrigEx, KeyToggleState);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
OurExRegisterKeyNotify (
  IN  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN  EFI_KEY_DATA                       *KeyData,
  IN  EFI_KEY_NOTIFY_FUNCTION            KeyNotificationFunction,
  OUT VOID                               **NotifyHandle
  )
{
  if (mOrigEx.RegisterKeyNotify != NULL) {
    return mOrigEx.RegisterKeyNotify (
                     &mOrigEx,
                     KeyData,
                     KeyNotificationFunction,
                     NotifyHandle
                     );
  }

  return EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
EFIAPI
OurExUnregisterKeyNotify (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN VOID                               *NotifyHandle
  )
{
  if (mOrigEx.UnregisterKeyNotify != NULL) {
    return mOrigEx.UnregisterKeyNotify (&mOrigEx, NotifyHandle);
  }

  return EFI_UNSUPPORTED;
}

//
// Legacy ConIn wrapper, kept for anything that still reads gST->ConIn.
//
STATIC
EFI_STATUS
EFIAPI
ButtonKeyReset (
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  IN BOOLEAN                         ExtendedVerification
  )
{
  mQueueHead = 0;
  mQueueTail = 0;

  if ((mOrigConIn != NULL) && (mOrigConIn->Reset != NULL)) {
    return mOrigConIn->Reset (mOrigConIn, ExtendedVerification);
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
ButtonKeyReadKeyStroke (
  IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  OUT EFI_INPUT_KEY                   *Key
  )
{
  PollButtons ();

  if (QueuePop (Key)) {
    return EFI_SUCCESS;
  }

  if ((mOrigConIn != NULL) && (mOrigConIn->ReadKeyStroke != NULL)) {
    return mOrigConIn->ReadKeyStroke (mOrigConIn, Key);
  }

  return EFI_NOT_READY;
}

EFI_STATUS
EFIAPI
ButtonKeyEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       Count;
  UINTN       Index;
  BOOLEAN     Wrapped;

  Handles    = NULL;
  Count      = 0;
  Wrapped    = FALSE;
  mOrigConIn = gST->ConIn;
  mOut       = gST->ConOut;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleTextInputExProtocolGuid,
                  NULL,
                  &Count,
                  &Handles
                  );
  if (EFI_ERROR (Status) || (Count == 0)) {
    DEBUG ((DEBUG_WARN, "[ButtonKey] no SimpleTextInputEx provider\n"));
    return EFI_SUCCESS;
  }

  for (Index = 0; Index < Count; Index++) {
    //
    // Never reinstall on a console handle: ConSplitter owns the aggregate
    // SimpleTextInputEx there, and replacing it corrupts its child
    // registrations ("CR has Bad Signature" assert) as well as recursing
    // through the aggregate back into this driver. Wrap the hardware
    // provider only, and only the first one.
    //
    if ((Handles[Index] == gST->ConsoleInHandle) ||
        (Handles[Index] == gST->ConsoleOutHandle) ||
        (Handles[Index] == gST->StandardErrorHandle))
    {
      DEBUG ((DEBUG_ERROR, "[ButtonKey] skipping console handle %p\n", Handles[Index]));
      continue;
    }

    Status = gBS->HandleProtocol (
                    Handles[Index],
                    &gEfiSimpleTextInputExProtocolGuid,
                    (VOID **)&mOrigExPtr
                    );
    if (EFI_ERROR (Status) || (mOrigExPtr == NULL)) {
      continue;
    }

    if (mOrigExPtr->ReadKeyStrokeEx == OurExReadKeyStrokeEx) {
      continue; // already ours
    }

    CopyMem (&mOrigEx, mOrigExPtr, sizeof (mOrigEx));

    Status = gBS->CreateEvent (0, TPL_CALLBACK, NULL, NULL, &mWaitEvent);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->CreateEvent (
                    EVT_TIMER | EVT_NOTIFY_SIGNAL,
                    TPL_CALLBACK,
                    ButtonKeyTimer,
                    NULL,
                    &mTimerEvent
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = gBS->SetTimer (mTimerEvent, TimerPeriodic, 500000); // 50 ms
    if (EFI_ERROR (Status)) {
      return Status;
    }

    mOurEx.Reset               = OurExReset;
    mOurEx.ReadKeyStrokeEx     = OurExReadKeyStrokeEx;
    mOurEx.WaitForKeyEx        = mWaitEvent;
    mOurEx.SetState            = OurExSetState;
    mOurEx.RegisterKeyNotify   = OurExRegisterKeyNotify;
    mOurEx.UnregisterKeyNotify = OurExUnregisterKeyNotify;

    Status = gBS->ReinstallProtocolInterface (
                    Handles[Index],
                    &gEfiSimpleTextInputExProtocolGuid,
                    mOrigExPtr,
                    &mOurEx
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[ButtonKey] reinstall Ex failed %r\n", Status));
      continue;
    }

    Wrapped = TRUE;
    DEBUG ((DEBUG_ERROR, "[ButtonKey] Ex wrapped on handle %p\n", Handles[Index]));

    if (mOut != NULL) {
      LineReset ();
      LinePut (L"BK: SimpleTextInputEx wrapped");
      LineFlush ();
    }

    break;   // one hardware provider is enough; more wrappers just chain
  }

  gBS->FreePool (Handles);

  if (!Wrapped) {
    return EFI_SUCCESS;
  }

  mOurConIn.Reset         = ButtonKeyReset;
  mOurConIn.ReadKeyStroke = ButtonKeyReadKeyStroke;
  mOurConIn.WaitForKey    = mWaitEvent;
  gST->ConIn              = &mOurConIn;

  return EFI_SUCCESS;
}
