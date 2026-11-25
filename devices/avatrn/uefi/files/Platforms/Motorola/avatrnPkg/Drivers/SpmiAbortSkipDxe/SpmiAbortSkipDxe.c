// Turns an SPMI-arbiter external abort into a failed read instead of a crash.
// Vendor SPMIDxe does "ldr w9,[x24]" on a PMIC channel's status register; on
// avatrn some channels (aDSP-owned, e.g. PM7250B at SID 8) bus-fault there.
// We skip the load, report "status = 0" (bit0 clear -> SPMIDxe returns error 1).
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Protocol/Cpu.h>

#define ARB_BASE 0x0C400000ULL
#define ARB_END  0x0C900000ULL

//
// Framebuffer beacon. "Display Reserved" is at 0xB8000000 (MemoryMapLib);
// panel is 1080x2400 BGRA8888. Writes a colored band at a known Y so we
// can see progress before GraphicsConsoleDxe installs ConOut.
//
#define FB_BASE    0xB8000000ULL
#define FB_WIDTH   1080
#define FB_HEIGHT  2400
#define FB_STRIDE  (FB_WIDTH * 4)
#define FB_BAND_H  12

//
// Off: these are full-width bands across the panel, painted during dispatch and
// never cleared. The 13:17 reference build paints nothing; re-enable to bisect.
//
STATIC CONST BOOLEAN  mFbBeacon = FALSE;

STATIC VOID
FbBeacon (
  IN UINT32  Slot,
  IN UINT32  Argb
  )
{
  volatile UINT32  *P;
  UINT32            x;
  UINT32            y;
  UINT32            y0 = Slot * (FB_BAND_H + 4);

  if (!mFbBeacon) {
    return;
  }

  if (y0 + FB_BAND_H >= FB_HEIGHT) {
    return;
  }

  for (y = 0; y < FB_BAND_H; y++) {
    P = (volatile UINT32 *)(UINTN)(FB_BASE + (y0 + y) * FB_STRIDE);
    for (x = 0; x < FB_WIDTH; x++) {
      P[x] = Argb;
    }
  }
}

//
// Fault record. Written here when the handler takes a fault it does not own,
// read back by the boot manager on the next boot and copied into
// \avatrn-dump.txt, so a fault after ExitBootServices still leaves a record.
// Both sides probe the page first (AT S1E1W / S1E1R), so an address that is not
// mapped in the current address space is skipped instead of faulting again.
//
#define FAULT_RECORD_BASE   0xF3FFF000ULL
#define FAULT_RECORD_MAGIC  0x4641554C54414348ULL  // "FAULTC(H)"

STATIC BOOLEAN
ProbeWrite (
  IN UINT64  Address
  )
{
  UINT64  Par;

  __asm__ volatile ("at s1e1w, %0" : : "r" (Address));
  __asm__ volatile ("isb");
  __asm__ volatile ("mrs %0, par_el1" : "=r" (Par));

  return (BOOLEAN)((Par & 1) == 0);
}

STATIC VOID
SaveFaultRecord (
  IN EFI_SYSTEM_CONTEXT_AARCH64  *C,
  IN UINT32                       Esr,
  IN UINT64                       Far
  )
{
  volatile UINT64  *R = (volatile UINT64 *)(UINTN)FAULT_RECORD_BASE;

  if (!ProbeWrite (FAULT_RECORD_BASE)) {
    return;
  }

  if (R[0] == FAULT_RECORD_MAGIC) {
    return;  // keep the first fault of this boot
  }

  R[1]  = Esr;
  R[2]  = Far;
  R[3]  = C->ELR;
  R[4]  = C->LR;
  R[5]  = C->SP;
  R[6]  = C->SPSR;
  R[7]  = C->X0;
  R[8]  = C->X1;
  R[9]  = C->X2;
  R[10] = C->X3;
  R[0]  = FAULT_RECORD_MAGIC;
}

STATIC UINTN mSkipped;

//
// Post-EBS diagnostics. DxeCore clears ST->ConOut on the way out of
// ExitBootServices, so keep our own copy: the protocol object itself outlives
// that, which is what lets a fault taken by Windows code still be reported.
//
STATIC EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *mOut;
STATIC CHAR16                            mBuf[256];
STATIC UINTN                             mLen;

STATIC VOID
Reset (VOID)
{
  mLen      = 0;
  mBuf[0]   = 0;
}

STATIC VOID
Put (
  IN CONST CHAR16  *S
  )
{
  while ((*S != 0) && (mLen < 200)) {
    mBuf[mLen++] = *S++;
  }

  mBuf[mLen] = 0;
}

STATIC VOID
PutHex (
  IN UINT64  V
  )
{
  CONST CHAR16  *D = L"0123456789ABCDEF";
  CHAR16        B[19];
  UINTN         i;

  B[0] = L'0';
  B[1] = L'x';
  for (i = 0; i < 16; i++) {
    B[2 + i] = D[(V >> ((15 - i) * 4)) & 0xF];
  }

  B[18] = 0;
  Put (B);
}

STATIC VOID
Flush (VOID)
{
  Put (L"\r\n");
  if (mOut != NULL) {
    mOut->OutputString (mOut, mBuf);
  }

  Reset ();
}


STATIC VOID
EFIAPI
SyncHandler (
  IN EFI_EXCEPTION_TYPE ExceptionType,
  IN EFI_SYSTEM_CONTEXT SystemContext
  )
{
  EFI_SYSTEM_CONTEXT_AARCH64 *C = SystemContext.SystemContextAArch64;
  UINT32 Esr = (UINT32)C->ESR;
  UINT64 Far = C->FAR;
  UINT32 Ec  = Esr >> 26;
  UINT32 Dfsc = Esr & 0x3F;

  // Data abort (same EL), synchronous external abort, inside the PMIC arbiter
  if (Ec == 0x25 && (Dfsc == 0x10 || (Dfsc >= 0x11 && Dfsc <= 0x17)) &&
      Far >= ARB_BASE && Far < ARB_END) {
    UINT32 Insn = *(volatile UINT32 *)C->ELR;
    // Single-register LDR/STR families (imm, reg-offset, unscaled/pre/post-index)
    BOOLEAN Single = ((Insn & 0x3B000000) == 0x39000000) ||
                     ((Insn & 0x3B200C00) == 0x38200800) ||
                     ((Insn & 0x3B200000) == 0x38000000);
    if (Single) {
      BOOLEAN IsLoad = ((Insn >> 22) & 1) != 0;
      if (IsLoad) {
        UINT32 Rt = Insn & 0x1F;
        // Channel status register (+0x8): report "done, OK" so a denied WRITE looks
        // successful. Every other skipped load reads as 0.
        if (Rt != 31) (&C->X0)[Rt] = ((Far & 0xFFF) == 0x8) ? 1 : 0;
      }
      // Stores are simply dropped.
      C->ELR += 4;
      mSkipped++;
      if (mSkipped <= 8) DEBUG ((DEBUG_WARN, "SpmiAbortSkip: skipped %a at %lx (FAR %lx) #%d\n", IsLoad ? "load" : "store", C->ELR - 4, Far, mSkipped));
      return;
    }
  }
  // Not ours: report it on screen as well, this fires on Windows code after EBS
  Reset ();
  Put (L"EXC: ESR ");
  PutHex ((UINT64)Esr);
  Put (L" EC ");
  PutHex ((UINT64)Ec);
  Put (L" FAR ");
  PutHex (Far);
  Flush ();

  Reset ();
  Put (L"EXC: ELR ");
  PutHex (C->ELR);
  Put (L" LR ");
  PutHex (C->LR);
  Flush ();

  Reset ();
  Put (L"EXC: SP ");
  PutHex (C->SP);
  Put (L" SPSR ");
  PutHex (C->SPSR);
  Flush ();

  SaveFaultRecord (C, Esr, Far);

  DEBUG ((DEBUG_ERROR, "Unhandled sync exception ESR %x FAR %lx ELR %lx\n", Esr, Far, C->ELR));
  DEBUG ((DEBUG_ERROR, "LR %lx SP %lx SPSR %lx\n", C->LR, C->SP, C->SPSR));
  DEBUG ((DEBUG_ERROR, "X0 %lx X1 %lx X2 %lx X3 %lx\n", C->X0, C->X1, C->X2, C->X3));

  //
  // x19-x28 are callee-saved, so winload's phase context is in there and not in
  // x0-x3. Two boots faulted at FAR = <word on the stack> + 0x144, the same
  // offset the EBS hook probes, so these say which register the faulting access
  // used as a base instead of inferring it from the address.
  //
  {
    UINT64 *X = &C->X0;

    DEBUG ((DEBUG_ERROR, "X19 %lx X20 %lx X21 %lx\n", X[19], X[20], X[21]));
    DEBUG ((DEBUG_ERROR, "X22 %lx X23 %lx X24 %lx\n", X[22], X[23], X[24]));
  }

  //
  // Which translation regime the fault was taken in. A level-0 fault (DFSC 4) on
  // a kernel address with TTBR1 pointing nowhere means the kernel half was never
  // installed; a TTBR1 pointing at real tables means only that 512 GB slot is
  // missing, which is a different bug entirely.
  //
  {
    UINT64 Ttbr0, Ttbr1, Tcr, Sctlr, El;

    __asm__ volatile ("mrs %0, ttbr0_el1" : "=r" (Ttbr0));
    __asm__ volatile ("mrs %0, ttbr1_el1" : "=r" (Ttbr1));
    __asm__ volatile ("mrs %0, tcr_el1"   : "=r" (Tcr));
    __asm__ volatile ("mrs %0, sctlr_el1" : "=r" (Sctlr));
    __asm__ volatile ("mrs %0, CurrentEL" : "=r" (El));

    DEBUG ((DEBUG_ERROR, "TTBR0 %lx TTBR1 %lx\n", Ttbr0, Ttbr1));
    DEBUG ((DEBUG_ERROR, "TCR %lx SCTLR %lx EL%u\n", Tcr, Sctlr, (UINT32)((El >> 2) & 3)));
  }

  {
    UINT64 *Sp = (UINT64 *)C->SP;
    UINTN i;
    for (i = 0; i < 16; i += 4) {
      DEBUG ((DEBUG_ERROR, "%lx: %lx %lx %lx %lx\n", (UINT64)(Sp + i), Sp[i], Sp[i+1], Sp[i+2], Sp[i+3]));
    }
  }

  //
  // Printed last on purpose: the dump above scrolls the panel, and these two
  // lines decode the faulting access itself - base register and offset - so the
  // next boot says what the instruction at ELR actually read.
  //
  {
    UINT32 *I = (UINT32 *)(UINTN)(C->ELR - 16);
    UINTN   i;

    for (i = 0; i < 8; i += 4) {
      DEBUG ((DEBUG_ERROR, "INSN %lx: %x %x %x %x\n",
        (UINT64)(UINTN)(I + i), I[i], I[i + 1], I[i + 2], I[i + 3]));
    }
  }

  //
  // If the fault was taken at EL2, the EL1 registers dumped above are not the
  // regime the fault came from: with HCR_EL2.E2H=0 the CPU translates EL2
  // accesses through TTBR0_EL2/TCR_EL2/SCTLR_EL2, and a level-0 fault there has
  // nothing to do with TTBR1_EL1. These are undefined at EL1 and would trap, so
  // they are read only when CurrentEL says EL2.
  //
  {
    UINT64 El;

    __asm__ volatile ("mrs %0, CurrentEL" : "=r" (El));
    if (((El >> 2) & 3) == 2) {
      UINT64 Ttbr0E2, TcrE2, SctlrE2, Hcr, VbarE2;

      __asm__ volatile ("mrs %0, ttbr0_el2" : "=r" (Ttbr0E2));
      __asm__ volatile ("mrs %0, tcr_el2"   : "=r" (TcrE2));
      __asm__ volatile ("mrs %0, sctlr_el2" : "=r" (SctlrE2));
      __asm__ volatile ("mrs %0, hcr_el2"   : "=r" (Hcr));
      __asm__ volatile ("mrs %0, vbar_el2"  : "=r" (VbarE2));

      DEBUG ((DEBUG_ERROR, "E2 TTBR0 %lx TCR %lx E2H %u\n",
        Ttbr0E2, TcrE2, (UINT32)((Hcr >> 34) & 1)));
      DEBUG ((DEBUG_ERROR, "E2 SCTLR %lx HCR %lx VBAR %lx\n",
        SctlrE2, Hcr, VbarE2));
    }
  }
  while (1) {}
}

EFI_STATUS
EFIAPI
SpmiAbortSkipEntry (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_CPU_ARCH_PROTOCOL *Cpu;
  EFI_STATUS Status;

  FbBeacon (0, 0x00FFFF00);  // slot 0: yellow - entry reached

  mOut   = gST->ConOut;
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&Cpu);
  if (EFI_ERROR (Status)) {
    FbBeacon (1, 0x00FF0000);  // slot 1: red - LocateProtocol failed
    return Status;
  }

  FbBeacon (1, 0x0000FFFF);    // slot 1: cyan - CpuArch acquired

  // Replace any existing handler for exception 0 (synchronous)
  Cpu->RegisterInterruptHandler (Cpu, 0, NULL);
  Status = Cpu->RegisterInterruptHandler (Cpu, 0, SyncHandler);

  FbBeacon (2, EFI_ERROR (Status) ? 0x00FF0000 : 0x0000FF00);  // slot 2: green ok / red fail
  return Status;
}
