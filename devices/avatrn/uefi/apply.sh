#!/bin/bash
# avatrn on HakiOS's UEFI: uefi/apply.sh (Mu-Silicium at uefi/BASE with HakiOS's changes) with this phone's
# platform, then the two patches here. The result is Mu-Silicium/ at the top of the repository.
set -e
HERE=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
MU=${MU:-$ROOT/Mu-Silicium}
MU="$MU" "$ROOT/uefi/apply.sh" avatrn
git -C "$MU" apply --whitespace=nowarn "$HERE/avatrn.patch"
git -C "$MU/Mu_Basecore" apply --whitespace=nowarn "$HERE/mu_basecore.patch"
echo "devices/avatrn/uefi/apply.sh: done. Still needed, from your own phone: see devices/avatrn/uefi/README.md"
